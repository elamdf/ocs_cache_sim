#include "ocs_cache.h"
#include <algorithm>
class BasicOCSCache : public OCSCache {

public:
  BasicOCSCache(int num_pools, int pool_size_bytes, int max_concurrent_pools)
      : OCSCache(num_pools, pool_size_bytes, max_concurrent_pools) {}

protected:
  BasicOCSCache::Status updateClustering(uint64_t addr,
                                         bool is_clustering_candidate) {
    candidate_cluster *candidate = nullptr;
    RETURN_IF_ERROR(is_clustering_candidate ? getOrCreateCandidate(addr, &candidate)
                                            : getCandidateIfExists(addr, &candidate));
    if (candidate != nullptr) {
      if (addrInRange(candidate->range, addr)) {
        candidate->on_cluster_accesses++;
      } else {
        candidate->off_cluster_accesses++;
        // TODO we need some way to remove candidates if they suck, otherwise we
        // will always short circuit on them.
      }
    }
    // TODO should we update the bounds of a candidate based on more complex
    // access insights?
    RETURN_IF_ERROR(materializeIfEligible(candidate));
    return Status::OK;
  }

  BasicOCSCache::Status runReplacement(uint64_t addr) {
    bool in_cache;
    RETURN_IF_ERROR(addrInCacheOrDram(addr, &in_cache));
    if (addrAlwaysInDRAM(addr) || in_cache) {
      return Status::BAD;
    }

    pool_node **to_swap_in = nullptr;
    RETURN_IF_ERROR(getPoolNode(addr, to_swap_in));
    if (to_swap_in == nullptr) { // this *should* be redundant?
      return Status::BAD;
    }

    // random eviction for now
    int idx_to_evict = random() % cache_size;
    cached_pools.assign(idx_to_evict, *to_swap_in);
    return Status::OK;
  }

  Status createCandidate(uint64_t addr_t, candidate_cluster **candidate) {
    *candidate = (candidate_cluster *)malloc(sizeof(candidate_cluster));
    if (*candidate == nullptr) {
      return Status::BAD;
    }
    // naive strategy, this only works if all candidates are initially created
    // at the bottom of their address range
    // TODO inlie this
    addr_subspace s = {addr_t, addr_t + pool_size_bytes};
    (*candidate)->range = s;
    (*candidate)->on_cluster_accesses = 0;
    (*candidate)->off_cluster_accesses = 0;
    (*candidate)->valid = true;
    return Status::OK;
  }

  // basically random for now
  bool eligibleForMaterialization(const candidate_cluster &candidate) {
      return candidate.valid && candidate.on_cluster_accesses  > 100 && candidate.on_cluster_accesses > 10 * candidate.off_cluster_accesses;
  }

  Status materializeIfEligible(candidate_cluster *candidate) {
      // TODO
      return Status::BAD;
  }
};
