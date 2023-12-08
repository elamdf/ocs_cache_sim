#include "ocs_cache.h"
// TODO add [[nodiscard]] to all functions (esp ones that return Status) or
// figure out some hacky macro way to do it
#include "ocs_structs.h"

#include <cstdlib>
#include <exception>
#include <iostream>
#include <sstream>

OCSCache::OCSCache(int num_pools, int pool_size_bytes,
                   int max_concurrent_ocs_pools, int backing_store_cache_size)
    : num_ocs_pools(num_pools), pool_size_bytes(pool_size_bytes),
      ocs_cache_size(max_concurrent_ocs_pools),
      backing_store_cache_size(backing_store_cache_size) {}

OCSCache::~OCSCache() {
  for (candidate_cluster *c : candidates) {
    free(c);
  }
  for (pool_entry *p : pools) {
    free(p);
  }
}

bool OCSCache::accessInRange(addr_subspace &range, mem_access access) {
  return access.addr >= range.addr_start &&
         access.addr + access.size < range.addr_end;
}

[[nodiscard]] OCSCache::Status
OCSCache::getCandidateIfExists(mem_access access,
                               candidate_cluster **candidate) {
  *candidate = nullptr;
  for (candidate_cluster *cand : candidates) {
    if (accessInRange(cand->range, access) && cand->valid) {
      *candidate = cand;
      break;
    }
  }

  return Status::OK;
}

[[nodiscard]] OCSCache::Status OCSCache::getPoolNode(mem_access access,
                                                     pool_entry **node) {
  // Iterate through the pool nodes backwards, since OCS nodes are appended to
  // the end and we want to prioritize them
  *node = nullptr;
  for (auto pool = pools.rbegin(); pool != pools.rend(); ++pool) {

    if (accessInRange((*pool)->range, access) && (*pool)->valid) {
      *node = (*pool);
      break;
    }
  }
  if (*node == nullptr &&
      !addrAlwaysInDRAM(access)) { // no node contains this address, and
                                   // it's not forced to be in DRAM.
                                   // lazily allocate a page-sized FM node

    candidate_cluster new_fm_page;
    new_fm_page.range.addr_start = PAGE_ALIGN_ADDR(access.addr);
    new_fm_page.range.addr_end = PAGE_ALIGN_ADDR(access.addr) + PAGE_SIZE;

    RETURN_IF_ERROR(
        createPoolFromCandidate(new_fm_page, node, /*is_ocs_node=*/false));
  }

  return Status::OK;
}

[[nodiscard]] OCSCache::Status OCSCache::poolNodeInCache(const pool_entry &node,
                                                         bool *in_cache) {
  // TODO there's probably a nice iterator /std::vector way to do this

  std::vector<pool_entry *> *cache =
      node.is_ocs_pool ? &cached_ocs_pools : &cached_backing_store_pools;

  for (pool_entry *pool : *cache) {
    DEBUG_CHECK(pool->is_ocs_pool == node.is_ocs_pool,
                "non ocs node found in ocs cache");
    if (node == *pool && node.valid) {
      *in_cache = true;
    }
  }

  return Status::OK;
}

[[nodiscard]] OCSCache::Status OCSCache::handleMemoryAccess(
    // TODO this needs to take size of access, we're ignoring alignment issues
    // rn
    mem_access access, bool *hit) {

  pool_entry *associated_node = nullptr;
  DEBUG_LOG("handling access to " << access.addr << " with stats " << *this
                                  << "\n");
  RETURN_IF_ERROR(accessInCacheOrDram(access, &associated_node, hit));
  DEBUG_CHECK(
      associated_node != nullptr,
      "there is no associated node with this access! a backing store node "
      "should have been materialized..."); // should either be a hit, or in an
                                           // uncached backing store node/ocs
                                           // node

  bool is_clustering_candidate = false;

  // TODO find a way to show the number of non-stack DRAM accesses going down
  // over time maybe have some histogram for saving stats going on every mem
  // access or something

  if (*hit) {
    if (addrAlwaysInDRAM(access)) {
      DEBUG_LOG("DRAM hit");
      stats.dram_hits++;
    } else {
      DEBUG_CHECK(associated_node->in_cache,
                  "hit on a node that's not marked as in cache!");
      if (associated_node->is_ocs_pool) {
        DEBUG_LOG("ocs hit on node " << associated_node->id);
        stats.ocs_pool_hits++;
      } else {
        DEBUG_LOG("backing store hit on node " << associated_node->id);
        stats.backing_store_pool_hits++;
        if (!addrAlwaysInDRAM(access)) {
          // this address is eligible to be in a pool, but is not currently in
          // one
          is_clustering_candidate = true;
        }
      }
    }
  } else {
    DEBUG_CHECK(!associated_node->in_cache,
                "missed on a node that's marked as in cache!");
    DEBUG_LOG("miss with associated node: \n" << *associated_node << std::endl);
    // the address is in an uncached (ocs or backing store) pool.
    if (associated_node->is_ocs_pool) {
      // the address is in an ocs pool, just not a cached one
      // Run replacement to cache its pool.
      stats.ocs_reconfigurations++;
      // TODO print out timestamp here for visualization?
      RETURN_IF_ERROR(runOCSReplacement(access));
    } else {
      // the address is in a backing store pool, just not a cached one.
      // Run replacement to cache its pool.
      stats.backing_store_misses++;
      RETURN_IF_ERROR(runBackingStoreReplacement(access));

      // this address is eligible to be in a pool, but is not currently in
      // one (it is in a backing store node)
      is_clustering_candidate = true;
    }
  }

  // we are committing to not updating a cluster once it's been chosen, for
  // now.
  RETURN_IF_ERROR(updateClustering(access, is_clustering_candidate));

  stats.accesses++;
  return Status::OK;
}

[[nodiscard]] OCSCache::Status
OCSCache::getOrCreateCandidate(mem_access access,
                               candidate_cluster **candidate) {
  RETURN_IF_ERROR(getCandidateIfExists(access, candidate));
  if (*candidate == nullptr) { // candidate doesn't exist, create one
    RETURN_IF_ERROR(createCandidate(access, candidate));
    candidates.push_back(*candidate);
  }
  return Status::OK;
}

OCSCache::Status
OCSCache::createPoolFromCandidate(const candidate_cluster &candidate,
                                  pool_entry **pool, bool is_ocs_node) {

  // FIXME this doesn't invalidate pages from the backing store that this draws
  // from
  pool_entry *new_pool_entry = (pool_entry *)malloc(sizeof(pool_entry));

  new_pool_entry->valid = true;
  new_pool_entry->is_ocs_pool = is_ocs_node;
  new_pool_entry->id = pools.size();
  new_pool_entry->range.addr_start = candidate.range.addr_start;
  new_pool_entry->range.addr_end = candidate.range.addr_end;

  if (is_ocs_node) {
    DEBUG_LOG("materializing OCS pool with address range "
              << new_pool_entry->range.addr_start << ":"
              << new_pool_entry->range.addr_end << std::endl);

    // invalidate backing store pages that fall completely within the range
    // covered by this pool. This may be overly safe

    // Round up start address to the nearest page alignment
    uintptr_t firstPage =
        (new_pool_entry->range.addr_start + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

    // Iterate (and invalidate) every backing store pool that falls completely
    // within this new pool's range
    for (uintptr_t current = firstPage;
         current < new_pool_entry->range.addr_end; current += PAGE_SIZE) {
      mem_access a;
      a.size = 1;
      a.addr = current;
      pool_entry *node;
      RETURN_IF_ERROR(getPoolNode(a, &node));
      DEBUG_CHECK(!node->is_ocs_pool,
                  "overlapping coverage of OCS pool ranges: "
                      << *new_pool_entry << "\nis trying to invalidate"
                      << *node);
      DEBUG_LOG(
          "Invalidating Backing store node: "
          << node->range
          << " due to it being covered by newly materialized OCS pool with id: "
          << new_pool_entry->id << std::endl);
      node->valid = false;
    }

  } else {
    DEBUG_LOG("materializing backing store pool with address range "
              << new_pool_entry->range.addr_start << ":"
              << new_pool_entry->range.addr_end << std::endl);
  }

  pools.push_back(new_pool_entry);
  *pool = new_pool_entry;

  return Status::OK;
}

[[nodiscard]] OCSCache::Status
OCSCache::materializeIfEligible(candidate_cluster *candidate) {
  if (eligibleForMaterialization(*candidate)) {
    pool_entry *throwaway;
    RETURN_IF_ERROR(
        createPoolFromCandidate(*candidate, &throwaway, /*is_ocs_node=*/true));
    candidate->valid = false;
    // TODO invalidate backing stores here or somehow figure out how to
    // prioritize ocs pools over backing pool nodes
    stats.candidates_promoted++;
  }
  return Status::OK;
}

std::ostream &operator<<(std::ostream &oss, const OCSCache &entry) {
  // Adding information about cached_pools

  oss << "Candidates:\n";
  for (const candidate_cluster *candidate : entry.candidates) {
    // Assuming candidate_cluster has a method to get a string representation
    if (candidate->valid || DEBUG) {
      oss << *candidate << "\n";
    }
  }

  oss << "Pools: \n";
  for (const pool_entry *pool : entry.pools) {
    // Assuming pool_entry has a method to get a string representation
    if (pool->valid || DEBUG) {
      oss << *pool << "\n";
    }
  }

  oss << "OCS Cache: \n";
  for (const pool_entry *pool : entry.cached_ocs_pools) {
    // Assuming pool_entry has a method to get a string representation
    if (pool->valid || DEBUG) {
      oss << *pool << "\n";
    }
  }

  oss << "Backing Store Cache: \n";
  for (const pool_entry *pool : entry.cached_backing_store_pools) {
    // Assuming pool_entry has a method to get a string representation
    if (pool->valid || DEBUG) {
      oss << *pool << "\n";
    }
  }

  return oss;
}

[[nodiscard]] OCSCache::Status
OCSCache::runReplacement(mem_access access, bool is_ocs_replacement) {
  bool in_cache = false;

  pool_entry *parent_pool = nullptr;

  // TODO figure out why this somehow floods the cache with the same node
  RETURN_IF_ERROR(accessInCacheOrDram(
      access, &parent_pool,
      &in_cache)); // FIXME this is inefficient bc we're already
                   // calling accessInCacheOrDram to decide if we want
                   // to call this functiona, but we still need to sanity
                   // check that it's not cached
  if (addrAlwaysInDRAM(access) || in_cache) {
    return Status::BAD;
  }

  if (parent_pool == nullptr ||
      parent_pool->is_ocs_pool != is_ocs_replacement) {
    return Status::BAD;
  }

  int cache_size =
      is_ocs_replacement ? ocs_cache_size : backing_store_cache_size;
  std::vector<pool_entry *> &cache =
      is_ocs_replacement ? cached_ocs_pools : cached_backing_store_pools;
  if (cache.size() < cache_size) {
    cache.push_back(parent_pool);
  } else {
    // random eviction for now TODO do LRU
    int idx_to_evict = random() % cache_size;
    DEBUG_LOG("evicting node " << cache[idx_to_evict]->id)
    cache[idx_to_evict]->in_cache = false;
    cache.assign(idx_to_evict, parent_pool);
  }
  parent_pool->in_cache = true;

  return Status::OK;
}

[[nodiscard]] OCSCache::Status OCSCache::accessInCacheOrDram(mem_access access,
                                                             pool_entry **node,
                                                             bool *in_cache) {
  RETURN_IF_ERROR(getPoolNode(access, node));
  if (*node == nullptr) {
    *in_cache = false;
  } else {
    DEBUG_LOG("checking if node " << (*node)->id << " is in cache\n")
    RETURN_IF_ERROR(poolNodeInCache(**node, in_cache));
  }

  *in_cache = *in_cache || addrAlwaysInDRAM(access);

  return Status::OK;
}

perf_stats OCSCache::getPerformanceStats(bool summary) {
  // reset stats before setting them
  stats.ocs_pool_mem_usage = 0;
  stats.backing_store_mem_usage = 0;
  stats.num_ocs_pools = 0;
  stats.num_backing_store_pools = 0;

  for (const pool_entry *entry : pools) {
    if (entry->valid) {
      if (entry->is_ocs_pool) {
        stats.ocs_pool_mem_usage += entry->size();
        stats.num_ocs_pools++;
      } else {
        stats.backing_store_mem_usage += entry->size();
        stats.num_backing_store_pools++;
      }
    }
  }

  stats.summary = summary; // effects << operator's verbosity
  return stats;
}
