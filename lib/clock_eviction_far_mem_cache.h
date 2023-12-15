#pragma once

#include "far_memory_cache.h"
#include "ocs_cache_sim/lib/clock_eviction_ocs_cache.h"

// TODO this should just inherit from ClockOCSCache too  but multiple
// inheritence is scary and we're crunched for time
class ClockFMCache : public ClockOCSCache {
public:
  ClockFMCache(int backing_store_cache_size)
      : ClockOCSCache(0, 0, backing_store_cache_size) {}

  [[nodiscard]] Status updateClustering(mem_access access,
                                        bool is_clustering_candidate) {
    return Status::OK;
  }

  [[nodiscard]] Status createCandidate(mem_access access,
                                       candidate_cluster **candidate) {
    return Status::OK;
  }

  bool eligibleForMaterialization(const candidate_cluster &candidate) {
    return false;
  };

  std::string getName() {
    return "Pure Far-Memory Cache with clock replacement";
  }
};
