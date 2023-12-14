#pragma once

#include "ocs_cache.h"
#include "ocs_cache_sim/lib/basic_ocs_cache.h"
#include "ocs_structs.h"
#include <algorithm>

class LiberalOCSCache: public BasicOCSCache {

public:
  LiberalOCSCache(int num_pools, int pool_size_bytes,
                int max_concurrent_ocs_pools, int backing_store_cache_size)
      : BasicOCSCache(num_pools, pool_size_bytes, max_concurrent_ocs_pools,
                 backing_store_cache_size) {}

protected:
  [[nodiscard]] Status updateClustering(mem_access access,
                                        bool is_clustering_candidate) override {
    candidate_cluster *candidate = nullptr;
    RETURN_IF_ERROR(is_clustering_candidate
                        ? getOrCreateCandidate(access, &candidate)
                        : getCandidateIfExists(access, &candidate));
    for (size_t idx = 0; idx < candidates.size(); idx++) {
      auto candidate = candidates[idx];
      if (candidate->valid) {
        if (accessInRange(candidate->range, access)) {
          candidate->on_cluster_accesses++;
        } else {
          candidate->off_cluster_accesses++;
        }
        if (candidate->off_cluster_accesses >
            2 * candidate->on_cluster_accesses) {
          candidate->valid = false;
        }
      }
    }

    RETURN_IF_ERROR(materializeIfEligible(candidate));
    return Status::OK;
  }

  // basically random for now
  bool eligibleForMaterialization(const candidate_cluster &candidate) override {
    return candidate.valid && candidate.on_cluster_accesses > 100 &&
           candidate.on_cluster_accesses > 2 * candidate.off_cluster_accesses;
  }

  std::string getName() const override {
    return "OCS cache with random liberal replacement for both NFM and backing store";
  }
};
