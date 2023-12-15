#pragma once

#include "ocs_cache.h"
#include "ocs_cache_sim/lib/basic_ocs_cache.h"
#include "ocs_cache_sim/lib/clock_eviction_ocs_cache.h"
#include "ocs_structs.h"
#include <algorithm>

// TODO this class hirearchy is all bad an ugly but we need otbe done
class LiberalClockOCSCache : public ClockOCSCache {

public:
  LiberalClockOCSCache(int pool_size_bytes, int max_concurrent_ocs_pools,
                        int backing_store_cache_size)
      : ClockOCSCache(pool_size_bytes, max_concurrent_ocs_pools,
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

  bool eligibleForMaterialization(const candidate_cluster &candidate) override {
    return candidate.valid && candidate.on_cluster_accesses > 100 &&
           candidate.on_cluster_accesses > 2 * candidate.off_cluster_accesses;
  }

  std::string getName() override {
    return "OCS cache with liberal clustering and clock replacement for both "
           "NFM and backing "
           "stores";
  }
};
