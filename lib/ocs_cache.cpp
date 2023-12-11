#include "ocs_cache.h"
#include "ocs_structs.h"
#include "utils.h"

#include <cstdlib>
#include <exception>
#include <iostream>
#include <sstream>

OCSCache::OCSCache(int num_pools, int pool_size_bytes,
                   int max_concurrent_ocs_pools, int backing_store_cache_size)
    : num_ocs_pools(num_pools), pool_size_bytes(pool_size_bytes),
      max_ocs_cache_size(max_concurrent_ocs_pools),
      max_backing_store_cache_size(backing_store_cache_size) {}

OCSCache::~OCSCache() {
  for (candidate_cluster *c : candidates) {
    free(c);
  }
  for (pool_entry *p : pools) {
    free(p);
  }
}

// an access touches a `range` if either
// its beginning or end is within the `range`, or the `access` spans beyond both
// endpoints of `range`.
bool OCSCache::accessInRange(addr_subspace &range, mem_access access) {
  return (access.addr >= range.addr_start && access.addr < range.addr_end) ||
         (access.addr + access.size >= range.addr_start &&
          access.addr + access.size < range.addr_end) ||
         (access.addr <= range.addr_start &&
          access.addr + access.size >= range.addr_end);

  ;
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

[[nodiscard]] OCSCache::Status
OCSCache::getPoolNodes(mem_access access,
                       std::vector<pool_entry *> *parent_pools) {
  // Iterate through the pool nodes backwards, since OCS nodes are appended to
  // the end and we want to prioritize them
  for (auto pool = pools.rbegin(); pool != pools.rend(); ++pool) {
    if (accessInRange((*pool)->range, access) && (*pool)->valid) {
      parent_pools->push_back((*pool));
    }
  }

  auto uncovered_range = findUncoveredRanges(access, parent_pools);

  // TODO this only works with basic, contiguous backing store page allocations
  // since it only checks the bounds of the page ranges to determine what part
  // of an access is within a page
  if (!uncovered_range.empty() &&
      !addrAlwaysInDRAM(access)) { // no node contains this address, and
                                   // it's not forced to be in DRAM.
                                   // lazily allocate a page-sized FM node

    // TODO this is bad because it might allocate overlapping pages, but I think
    // it should only do that if the system was already incorrect?
    for (addr_subspace &range : uncovered_range) {
      for (uintptr_t base = PAGE_ALIGN_ADDR(range.addr_start);
           base < range.addr_end; base += PAGE_SIZE) {
        candidate_cluster new_fm_page;
        new_fm_page.range.addr_start = PAGE_ALIGN_ADDR(base);
        new_fm_page.range.addr_end = PAGE_ALIGN_ADDR(base) + PAGE_SIZE;

        pool_entry *tmp_pool_pointer = nullptr;

        RETURN_IF_ERROR(createPoolFromCandidate(new_fm_page, &tmp_pool_pointer,
                                                /*is_ocs_node=*/false));
        DEBUG_CHECK(tmp_pool_pointer != nullptr,
                    "createPoolFromCandidate returned null!");
        parent_pools->push_back(tmp_pool_pointer);
      }
    }
  }

  DEBUG_LOG("getPoolNodes got " << parent_pools->size() << " nodes\n");

  return Status::OK;
}

[[nodiscard]] OCSCache::Status
// pool_entry should be const ref prob, don't immediately know how to type
// massage that
OCSCache::poolNodesInCache(std::vector<pool_entry *> *nodes,
                           std::vector<bool> *in_cache) {
  // TODO there's probably a nice iterator /std::vector way to do this
  // TODO can we trust the in_cache member? I sure don't lol
  in_cache->clear();

  for (pool_entry *node : *nodes) {
    std::vector<pool_entry *> *cache =
        node->is_ocs_pool ? &cached_ocs_pools : &cached_backing_store_pools;

    in_cache->push_back(std::any_of(
        cache->begin(), cache->end(), [&node](pool_entry *pool) -> bool {
          DEBUG_CHECK(pool->is_ocs_pool == node->is_ocs_pool,
                      "non ocs node found in ocs cache");
          if (*node == *pool && node->valid) {
            return true;
          }
          return false;
        }));
  }

  return Status::OK;
}

[[nodiscard]] OCSCache::Status OCSCache::handleMemoryAccess(
    // TODO this needs to take size of access, we're ignoring alignment issues
    // rn
    mem_access access, bool *hit) {
  *hit = false;

  std::vector<bool> node_hits;
  std::vector<pool_entry *> associated_nodes;
  DEBUG_LOG("handling access to " << access.addr << " with stats\n " << *this
                                  << "\n");
  RETURN_IF_ERROR(accessInCacheOrDram(access, &associated_nodes, &node_hits));
  DEBUG_CHECK(
      !associated_nodes.empty(),
      "there is no associated node with this access! a backing store node "
      "should have been materialized..."); // should either be a hit, or in an
                                           // uncached backing store node/ocs
                                           // node

  bool is_clustering_candidate = false;

  // TODO find a way to show the number of non-stack DRAM accesses going down
  // over time maybe have some histogram for saving stats going on every mem
  // access or something

  if (!node_hits.empty() && std::all_of(node_hits.begin(), node_hits.end(),
                                        [](bool val) { return val; })) {
    *hit = true; // We only consider a memory access a full cache hit if every
                 // parent node is cached
    if (addrAlwaysInDRAM(access)) {
      DEBUG_LOG("DRAM hit");
      stats.dram_hits++;
    } else {
      for (pool_entry *associated_node : associated_nodes) {
        DEBUG_CHECK(associated_node->in_cache,
                    "hit on a node that's not marked as in cache!");
        if (associated_node->is_ocs_pool) {
          DEBUG_LOG("ocs hit on node " << associated_node->id);
          stats.ocs_pool_hits++;
        } else {
          DEBUG_LOG("backing store hit on node " << associated_node->id);
          stats.backing_store_hits++;
          if (!addrAlwaysInDRAM(access)) {
            // this address is eligible to be in a pool, but is not currently in
            // one
            is_clustering_candidate = true;
          }
        }
      }
    }
    DEBUG_LOG("miss\n");
  } else {
    DEBUG_LOG("miss with associated nodes: \n"
              <<
              [&associated_nodes]() {
                std::ostringstream oss;
                for (const auto &elem : associated_nodes)
                  oss << *elem << '\n';
                return oss.str();
              }()
              << std::endl);

    DEBUG_CHECK(associated_nodes.empty() ||
                    !std::all_of(associated_nodes.begin(),
                                 associated_nodes.end(),
                                 [](auto e) { return e->in_cache; }),
                "missed when all nodes are marked as in cache!");

    // the address is in an uncached (ocs or backing store) pool.
    // TODO this is a dumb hack
    if (associated_nodes[0]->is_ocs_pool) {
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

  stats.accesses += associated_nodes.size();

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

  pool_entry *new_pool_entry = (pool_entry *)malloc(sizeof(pool_entry));

  new_pool_entry->valid = true;
  new_pool_entry->in_cache = false;
  new_pool_entry->is_ocs_pool = is_ocs_node;
  new_pool_entry->id = pools.size();
  new_pool_entry->range.addr_start = candidate.range.addr_start;
  new_pool_entry->range.addr_end = candidate.range.addr_end;

  if (is_ocs_node) {
    DEBUG_LOG("materializing OCS pool with address range "
              << new_pool_entry->range.addr_start << ":"
              << new_pool_entry->range.addr_end << std::endl);

    // invalidate only backing store pages that fall completely within the range
    // covered by this pool. This may be overly safe

    // Round up start address to the nearest page alignment
    uintptr_t firstPage =
        (new_pool_entry->range.addr_start + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

    // Iterate (and invalidate) every backing store pool that falls completely
    // within this new pool's range
    // note that this might materialize new backing nodes just to invalidate
    // them, but that's fine since it won't affect performance stats. Although
    // it's wasteful of cycles...
    for (uintptr_t current = firstPage;
         current < new_pool_entry->range.addr_end; current += PAGE_SIZE) {
      mem_access a;
      a.size = 1;
      a.addr = current;
      std::vector<pool_entry *> nodes;
      nodes.clear();
      RETURN_IF_ERROR(getPoolNodes(a, &nodes));

      // size 1, shouldn't have multiple matches
      DEBUG_CHECK(nodes.size() == 1,
                  "access of size 1 should only have one matching node! ");
      pool_entry *node = nodes[0];

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
// TODO this is not robust to access that touch nodes from both ocs and backing
// store pools is_ocs_replacement should be a vector
OCSCache::runReplacement(mem_access access, bool is_ocs_replacement) {
  std::vector<bool> nodes_in_cache;

  std::vector<pool_entry *> parent_pools;

  RETURN_IF_ERROR(accessInCacheOrDram(
      access, &parent_pools,
      &nodes_in_cache)); // FIXME this is inefficient bc we're already
                         // calling accessInCacheOrDram to decide if we want
                         // to call this functiona, but we still need to sanity
                         // check that it's not cached

  if (addrAlwaysInDRAM(access) ||
      std::all_of(nodes_in_cache.begin(), nodes_in_cache.end(),
                  [](bool a) { return a; })) {
    // we shouldn't have called replacement if everything is either in DRAM
    // access or in cache.
    // TODO this might not be robust to things on the very edge of stack
    // boundary but that is practically very unlikely to occur
    return Status::BAD;
  }

  if (parent_pools.empty() ||
      std::any_of(parent_pools.begin(), parent_pools.end(),
                  [is_ocs_replacement](auto e) {
                    return e->is_ocs_pool != is_ocs_replacement;
                  })) {
    return Status::BAD;
  }

  for (pool_entry *parent_pool : parent_pools) {
    int max_cache_size =
        is_ocs_replacement ? max_ocs_cache_size : max_backing_store_cache_size;
    std::vector<pool_entry *> &cache =
        is_ocs_replacement ? cached_ocs_pools : cached_backing_store_pools;

    int idx_to_evict = indexToReplace(is_ocs_replacement);

    DEBUG_LOG("index to evict is " << idx_to_evict);
    DEBUG_CHECK(idx_to_evict < max_cache_size,
                "idx_to_evict was bigger than max cache_size");
    if (cache.size() <= idx_to_evict) { // we are just exetending the cache

      DEBUG_CHECK(cache.size() == idx_to_evict,
                  "we only support compulsory misses being inserted at the end "
                  "of the current cache vector for now\n");
      cache.push_back(parent_pool);

    } else {
      DEBUG_LOG("evicting node " << cache[idx_to_evict]->id)
      cache[idx_to_evict]->in_cache = false;
      cache[idx_to_evict] = parent_pool;
    }

    DEBUG_CHECK(cache.size() <= max_cache_size,
                "cache was bigger than max_cache_size after replacement");
    parent_pool->in_cache = true;
  }

  return Status::OK;
}

[[nodiscard]] int OCSCache::indexToReplace(bool is_ocs_replacement) {
  // random eviction
  std::vector<pool_entry *> &cache =
      is_ocs_replacement ? cached_ocs_pools : cached_backing_store_pools;

  int max_cache_size =
      is_ocs_replacement ? max_ocs_cache_size : max_backing_store_cache_size;
  if (cache.size() < max_cache_size) {
    return cache.size();
  }
  return random() % max_cache_size;
}

[[nodiscard]] OCSCache::Status
OCSCache::accessInCacheOrDram(mem_access access,
                              std::vector<pool_entry *> *associated_nodes,
                              std::vector<bool> *in_cache) {

  in_cache->clear();
  RETURN_IF_ERROR(getPoolNodes(access, associated_nodes));
  if (!associated_nodes->empty()) {
    RETURN_IF_ERROR(poolNodesInCache(associated_nodes, in_cache));
    DEBUG_CHECK(
        associated_nodes->size() == in_cache->size(),
        "poolNodesInCache returned a different number of booleans than pools");

  } else if (addrAlwaysInDRAM(access)) {
    in_cache->push_back(true);
  } else {
    return Status::BAD;
  }

  return Status::OK;
}

perf_stats OCSCache::getPerformanceStats() {
  return getPerformanceStats(false);
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
