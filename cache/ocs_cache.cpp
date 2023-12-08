#include "ocs_cache.h"
// TODO add [[nodiscard]] to all functions (esp ones that return Status) or
// figure out some hacky macro way to do it
#include "ocs_structs.h"

#include <cstdlib>
#include <exception>
#include <iostream>
#include <sstream>

OCSCache::OCSCache(int num_pools, int pool_size_bytes, int max_concurrent_ocs_pools, int backing_store_cache_size)
    : num_ocs_pools(num_pools), pool_size_bytes(pool_size_bytes),
      ocs_cache_size(max_concurrent_ocs_pools), backing_store_cache_size(backing_store_cache_size) {}

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
  // TODO this assumes non-overlapping ranges between backing store pools + ocs
  // pools
  *node = nullptr;
  for (auto pool = pools.rbegin(); 
                  pool != pools.rend(); ++pool ) { 

    if (accessInRange((*pool)->range, access)) {
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
    DEBUG_CHECK(pool->is_ocs_pool == node.is_ocs_pool);
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
  DEBUG_LOG("handling access to " << access.addr << "\n");
  RETURN_IF_ERROR(accessInCacheOrDram(access, &associated_node, hit));
  DEBUG_CHECK(associated_node !=
              nullptr); // should either be a hit, or in an uncached backing
                        // store node/ocs node

  bool is_clustering_candidate = false;

  // TODO find a way to show the number of non-stack DRAM accesses going down
  // over time maybe have some histogram for saving stats going on every mem
  // access or something

  if (*hit) {
    if (addrAlwaysInDRAM(access)) {
      stats.dram_hits++;
    } else {
      if (associated_node->is_ocs_pool) {
        stats.ocs_pool_hits++;
      } else {
        stats.backing_store_pool_hits++;
        if (!addrAlwaysInDRAM(access)) {
          // this address is eligible to be in a pool, but is not currently in
          // one
          is_clustering_candidate = true;
        }
      }
    }
  } else {
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
  pool_entry *new_pool_entry = (pool_entry *)malloc(sizeof(pool_entry));

  new_pool_entry->valid = true;
  new_pool_entry->is_ocs_pool = is_ocs_node;
  new_pool_entry->id = pools.size();
  new_pool_entry->range.addr_start = candidate.range.addr_start;
  new_pool_entry->range.addr_end = candidate.range.addr_end;
  pools.push_back(new_pool_entry);

  *pool = new_pool_entry;

  return Status::OK;
}

[[nodiscard]] OCSCache::Status
OCSCache::materializeIfEligible(candidate_cluster *candidate) {
  if (eligibleForMaterialization(*candidate)) {
      pool_entry *throwaway;
    RETURN_IF_ERROR(createPoolFromCandidate(*candidate, &throwaway, /*is_ocs_node=*/true));
    candidate->valid = false;
    // TODO invalidate backing stores here or somehow figure out how to prioritize ocs pools over backing pool nodes
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

  oss << "Cache: \n";
  for (const pool_entry *pool : entry.cached_ocs_pools) {
    // Assuming pool_entry has a method to get a string representation
    if (pool->valid || DEBUG) {
      oss << *pool << "\n";
    }
  }

  return oss;
}

[[nodiscard]] OCSCache::Status OCSCache::accessInCacheOrDram(mem_access access,
                                                             pool_entry **node,
                                                             bool *in_cache) {
  RETURN_IF_ERROR(getPoolNode(access, node));
  if (*node == nullptr) {
    *in_cache = false;
  } else {
    RETURN_IF_ERROR(poolNodeInCache(**node, in_cache));
  }

  *in_cache = *in_cache || addrAlwaysInDRAM(access);

  return Status::OK;
}


perf_stats OCSCache::getPerformanceStats(bool summary) {
    long ocs_pool_mem_usage  = 0;
    long backing_store_mem_usage  = 0;

    for (const pool_entry * entry : pools) {
        if (entry->valid) {
            if (entry->is_ocs_pool) {
                ocs_pool_mem_usage += entry->size();
            } else {
                backing_store_mem_usage += entry->size();
            }
        }
    }

    stats.ocs_pool_mem_usage = ocs_pool_mem_usage;
    stats.backing_store_mem_usage = backing_store_mem_usage;

    stats.summary = summary; // effects << operator's verbosity
    return stats;
}
