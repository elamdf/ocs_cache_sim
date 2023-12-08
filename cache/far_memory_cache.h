
#pragma once

#include "ocs_cache.h"
#include "ocs_structs.h"
#include <algorithm>
class FarMemCache : public OCSCache {

public:
  FarMemCache(int backing_store_cache_size)
      : OCSCache(0, 0, 0, backing_store_cache_size) {}

protected:
  [[nodiscard]] Status
  updateClustering(mem_access access, bool is_clustering_candidate) {
    return Status::OK;
  }

  [[nodiscard]] Status runOCSReplacement(mem_access access) {
    return Status::OK;
  }

  [[nodiscard]] Status
  runBackingStoreReplacement(mem_access access) {
    RETURN_IF_ERROR(runReplacement(access, false));
    return Status::OK;
  }

  [[nodiscard]] Status createCandidate(mem_access access,
                                       candidate_cluster **candidate) {
    return Status::OK;
  }

  bool eligibleForMaterialization(const candidate_cluster &candidate) {
    return false;
  };

  std::string cacheName() const { return "Pure Far-Memory Cache"; }
};
