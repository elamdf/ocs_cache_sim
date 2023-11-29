#include "ocs_cache.h"
// TODO add [[nodiscard]] to all functions (esp ones that return Status) or
// figure out some hacky macro way to do it
#include "ocs_structs.h"

#include <cstdlib>
#include <exception>
#include <iostream>
#include <sstream>

OCSCache::OCSCache(int num_pools, int pool_size_bytes, int max_concurrent_pools)
    : num_pools(num_pools), pool_size_bytes(pool_size_bytes),
      cache_size(max_concurrent_pools) {}

OCSCache::~OCSCache() {
  for (candidate_cluster *c : candidates) {
    free(c);
  }
  for (pool_entry *p : pools) {
    free(p);
  }
}

bool OCSCache::accessInRange(addr_subspace &range, mem_access access) {
  // TODO this needs to take size into account, assumes access is 1 byte
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
  *node = nullptr;
  for (pool_entry *pool : pools) {
    if (accessInRange(pool->range, access)) {
      *node = pool;
      break;
    }
  }

  return Status::OK;
}

[[nodiscard]] OCSCache::Status OCSCache::poolNodeInCache(const pool_entry &node,
                                                         bool *in_cache) {
  // TODO there's probably a nice iterator /std::vector way to do this
  *in_cache = false;
  for (pool_entry *pool : cached_pools) {
    if (node == *pool) {
      *in_cache = true;
    }
  }

  return Status::OK;
}

[[nodiscard]] OCSCache::Status OCSCache::handleMemoryAccess(
    // TODO this needs to take size of access, we're ignoring alignment issues
    // rn
    mem_access access,
    bool *hit) { // TODO this could take the entire dyamorio memaccess
                 // struct? or maybe just add if this is a ld/st

  DEBUG_LOG("handling access to " << access.addr << "\n");
  RETURN_IF_ERROR(accessInCacheOrDram(access, hit));
  bool is_clustering_candidate = false;

  if (*hit) {
    stats.hits++;
  } else {
    stats.misses++;
    pool_entry *associated_node = nullptr;
    RETURN_IF_ERROR(getPoolNode(access, &associated_node));
    // we are committing to not updating a cluster once it's been chosen, for
    // now.
    if (associated_node != nullptr) {
      // this address exists in a pool, just not a cached one.
      // Run replacement to cache its pool.
      RETURN_IF_ERROR(runReplacement(access));
    } else {
      // this address doesn't yet exist in a pool.
      if (!addrAlwaysInDRAM(access)) {
        // this address is eligible to be in a pool.
        is_clustering_candidate = true;
      }
    }
  }
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
OCSCache::createPoolFromCandidate(const candidate_cluster &candidate) {
  pool_entry *new_pool_entry = (pool_entry *)malloc(sizeof(pool_entry));

  new_pool_entry->valid = true;
  new_pool_entry->id = pools.size();
  new_pool_entry->range.addr_start = candidate.range.addr_start;
  new_pool_entry->range.addr_end = candidate.range.addr_end;
  pools.push_back(new_pool_entry);

  return Status::OK;
}

[[nodiscard]] OCSCache::Status
OCSCache::materializeIfEligible(candidate_cluster *candidate) {
  if (eligibleForMaterialization(*candidate)) {
    RETURN_IF_ERROR(createPoolFromCandidate(*candidate));
    candidate->valid = false;
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
  for (const pool_entry *pool : entry.cached_pools) {
    // Assuming pool_entry has a method to get a string representation
    if (pool->valid || DEBUG) {
      oss << *pool << "\n";
    }
  }

  return oss;
}
