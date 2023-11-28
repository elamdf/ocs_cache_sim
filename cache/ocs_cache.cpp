#include "ocs_cache.h"
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

bool OCSCache::addrInRange(addr_subspace &range, uint64_t addr) {
  // TODO this needs to take size into account, assumes access is 1 byte
  return addr > range.addr_start && addr < range.addr_end;
}

OCSCache::Status OCSCache::getCandidateIfExists(uint64_t addr,
                                                candidate_cluster **candidate) {
  *candidate = nullptr;
  for (candidate_cluster *cand : candidates) {
    if (addrInRange(cand->range, addr)) {
      *candidate = cand;
      break;
    }
  }

  return Status::OK;
}

OCSCache::Status OCSCache::getPoolNode(uint64_t addr, pool_entry **node) {
  *node = nullptr;
  for (pool_entry *pool : pools) {
    if (addrInRange(pool->range, addr)) {
      *node = pool;
      break;
    }
  }

  return Status::OK;
}

OCSCache::Status OCSCache::poolNodeInCache(const pool_entry &node,
                                           bool *in_cache) {
  // TODO there's probably a nice iterator /std::vector way to do this
  *in_cache = false;
  for (pool_entry *pool : cached_pools) {
    if (&node == pool) {
      *in_cache = true;
    }
  }

  return Status::OK;
}

OCSCache::Status OCSCache::handleMemoryAccess(
    // TODO this needs to take size of access, we're ignoring alignment issues
    // rn
    uint64_t addr,
    bool *hit) { // TODO this could take the entire dyamorio memaccess
                 // struct? or maybe just add if this is a ld/st

  RETURN_IF_ERROR(addrInCacheOrDram(addr, hit));
  bool is_clustering_candidate = false;

  if (!*hit) {
    pool_entry *associated_node = nullptr;
    RETURN_IF_ERROR(getPoolNode(addr, &associated_node));
    // we are committing to not updating a cluster once it's been chosen, for
    // now.
    if (associated_node != nullptr) {
      // this address exists in a pool, just not a cached one.
      // Run replacement to cache its pool.
      runReplacement(addr);
    } else {
      // this address doesn't yet exist in a pool.
      if (!addrAlwaysInDRAM(addr)) {
        // this address is eligible to be in a pool.
        is_clustering_candidate = true;
      }
    }
  }
  updateClustering(addr, is_clustering_candidate);

  return Status::OK;
}

OCSCache::Status OCSCache::getOrCreateCandidate(uint64_t addr,
                                                candidate_cluster **candidate) {
  getCandidateIfExists(addr, candidate);
  if (*candidate == nullptr) { // candidate doesn't exist, create one
    createCandidate(addr, candidate);
    candidates.push_back(*candidate);
  }
  return Status::OK;
}

std::ostream &operator<<(std::ostream &oss, const OCSCache &entry) {
  // Adding information about cached_pools
  oss << "Cached Pools:\n";
  for (const pool_entry *pool : entry.cached_pools) {
    // Assuming pool_entry has a method to get a string representation
    oss << "  " << *pool << "\n";
  }

  // Adding information about candidates
  oss << "Candidates:\n";
  for (const candidate_cluster *candidate : entry.candidates) {
    // Assuming candidate_cluster has a method to get a string representation
    oss << "  " << *candidate << "\n";
  }

  return oss;
}
