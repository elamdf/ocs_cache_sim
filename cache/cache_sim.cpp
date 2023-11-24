#include "cache_sim.h"

#include <cstdlib>
#include <exception>
#include <iostream>

OCS::OCS(int num_pools, int pool_size_bytes,
         int max_concurrent_pools) { // todo that fancy c++ initialization thing
  pools = std::vector<pool_entry>(num_pools);
  num_pools = num_pools;
  pool_size_bytes = pool_size_bytes;
  cache_size = max_concurrent_pools;
}

OCS::Status OCS::updateClustering(uint64_t addr, bool is_clustering_candidate) {
  auto candidate = is_clustering_candidate ? getOrCreateCandidate(addr)
                                           : getCandidateIfExists(addr);
  if (candidate.valid) {
    if (addrInRange(candidate.range, addr)) {
      candidate.on_cluster_accesses++;
    } else {
      candidate.off_cluster_accesses++;
      // TODO we need some way to remove candidates if they suck, otherwise we
      // will always short circuit on them.
    }
  }
  // TODO should we update the bounds of a candidate based on more complex
  // access insights?
  materializeIfEligible(candidate);
  return Status::OK;
}

OCS::Status OCS::runReplacement(uint64_t addr) {
  bool in_cache;
  RETURN_IF_ERROR(addrInCacheOrDram(addr, &in_cache));
  if (addrAlwaysInDRAM(addr) || in_cache) {
    return Status::BAD;
  }

  pool_node *to_swap_in;
  RETURN_IF_ERROR(getPoolNode(addr, to_swap_in));
  if (to_swap_in == nullptr) { // this *should* be redundant?
    return Status::BAD;
  }

  // random eviction for now
  int idx_to_evict = random() % cache_size;
  cached_pools.assign(idx_to_evict, to_swap_in);
  return Status::OK;
}

bool OCS::addrInRange(addr_subspace &range, uint64_t addr) {
  // TODO this needs to take size into account, assumes access is 1 byte
  return addr > range.addr_start && addr < range.addr_end;
}

OCS::Status OCS::getPoolNode(uint64_t addr, pool_node *node) {
  node = nullptr;
  for (pool_node &pool : pools) {
    if (addrInRange(pool.range, addr)) {
      node = &pool;
      break;
    }
  }

  return Status::OK;
}

OCS::Status OCS::poolNodeInCache(const pool_node &node, bool *in_cache) {
  // TODO there's probably a nice iterator /std::vector way to do this
  *in_cache = false;
  for (pool_node *pool : cached_pools) {
    if (&node == pool) {
      *in_cache = true;
    }
  }

  return Status::OK;
}

OCS::Status OCS::handleMemoryAccess(
    // TODO this needs to take size of access, we're ignoring alignment issues
    // rn
    uint64_t addr,
    bool *hit) { // TODO this could take the entire dyamorio memaccess
                 // struct? or maybe just add if this is a ld/st

  RETURN_IF_ERROR(addrInCacheOrDram(addr, hit));
  bool is_clustering_candidate = false;

  if (!*hit) {
    pool_node *associated_node;
    RETURN_IF_ERROR(getPoolNode(addr, associated_node));
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

int main() { std::cout << "blah blah blah\n"; }
