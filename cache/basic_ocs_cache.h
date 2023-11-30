#pragma once

#include "ocs_cache.h"
#include "ocs_structs.h"
#include <algorithm>
class BasicOCSCache : public OCSCache {

public:
  BasicOCSCache(int num_pools, int pool_size_bytes, int max_concurrent_pools)
      : OCSCache(num_pools, pool_size_bytes, max_concurrent_pools) {}

protected:
  [[nodiscard]] BasicOCSCache::Status
  updateClustering(mem_access access, bool is_clustering_candidate) {
    candidate_cluster *candidate = nullptr;
    RETURN_IF_ERROR(is_clustering_candidate
                        ? getOrCreateCandidate(access, &candidate)
                        : getCandidateIfExists(access, &candidate));
    if (candidate != nullptr) {
        // TODO this will never increment off cluster accesses, we need to figure out how to switch to a new candidate
        // have one 'promotable' candidate at once, have some criteria to switch that (ie some number of contiguous off-cluster accesses)
      if (accessInRange(candidate->range, access)) {
        candidate->on_cluster_accesses++;
      } else {
        candidate->off_cluster_accesses++;
        // TODO we need some way to remove candidates if they suck, otherwise we
        // will always short circuit on them.
      }
      // TODO should we update the bounds of a candidate based on more complex
      // access insights?
      RETURN_IF_ERROR(materializeIfEligible(candidate));
    }
    return Status::OK;
  }

  [[nodiscard]] BasicOCSCache::Status runReplacement(mem_access access) {
    bool in_cache;

    RETURN_IF_ERROR(accessInCacheOrDram(access, &in_cache));
    if (addrAlwaysInDRAM(access) || in_cache) {
      return Status::BAD;
    }

    pool_entry *to_swap_in = nullptr;
    RETURN_IF_ERROR(getPoolNode(access, &to_swap_in));
    if (to_swap_in == nullptr) { // this *should* be redundant?
      return Status::BAD;
    }

    // random eviction for now TODO do LRU
    int idx_to_evict = random() % cache_size;
    if (cached_pools.size() > idx_to_evict &&
        cached_pools[idx_to_evict] != nullptr) {
      cached_pools[idx_to_evict]->in_cache = false;
    }

    cached_pools.assign(idx_to_evict, to_swap_in);
    to_swap_in->in_cache = true;
    return Status::OK;
  }

  [[nodiscard]] Status createCandidate(mem_access access,
                                       candidate_cluster **candidate) {
    *candidate = (candidate_cluster *)malloc(sizeof(candidate_cluster));
    if (*candidate == nullptr) {
      return Status::BAD;
    }

    // naive strategy, this only works if big accesses aren't one-offs
    addr_subspace s;
    if (access.size > pool_size_bytes * .5) {
      s.addr_start = access.addr;
      s.addr_end = access.addr + pool_size_bytes;
    } else {
      s.addr_start = access.addr - .5 * pool_size_bytes;
      s.addr_end = access.addr + .5 * pool_size_bytes;
    }

    (*candidate)->range = s;
    (*candidate)->id = candidates.size();
    (*candidate)->on_cluster_accesses = 0;
    (*candidate)->off_cluster_accesses = 0;
    (*candidate)->valid = true;

    stats.candidates_created++;
    return Status::OK;
  }

  // basically random for now
  bool eligibleForMaterialization(const candidate_cluster &candidate) {
    return candidate.valid && candidate.on_cluster_accesses > 100 &&
           candidate.on_cluster_accesses > 10 * candidate.off_cluster_accesses;
  }
};
