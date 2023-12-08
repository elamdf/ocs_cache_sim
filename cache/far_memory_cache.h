
#pragma once

#include "ocs_cache.h"
#include "ocs_structs.h"
#include <algorithm>
class FarMemCache : public OCSCache {

public:
  FarMemCache(int backing_store_cache_size)
      : OCSCache(0, 0, 0, backing_store_cache_size) {}

protected:
  [[nodiscard]] FarMemCache::Status
  updateClustering(mem_access access, bool is_clustering_candidate) {
    return Status::OK;
  }

  [[nodiscard]] FarMemCache::Status runOCSReplacement(mem_access access) {
    return Status::OK;
  }

  [[nodiscard]] FarMemCache::Status
  runBackingStoreReplacement(mem_access access) {
    RETURN_IF_ERROR(runReplacement(access, false));
    return Status::OK;
  }

  [[nodiscard]] FarMemCache::Status runReplacement(mem_access access,
                                                   bool is_ocs_replacement) {
    DEBUG_CHECK(!is_ocs_replacement);

    bool in_cache = false;

    pool_entry *parent_pool = nullptr;

    RETURN_IF_ERROR(accessInCacheOrDram(access, &parent_pool, &in_cache));

    if (addrAlwaysInDRAM(access) || in_cache) {
      return Status::BAD;
    }

    if (parent_pool == nullptr ||
        parent_pool->is_ocs_pool != is_ocs_replacement) {
      return Status::BAD;
    }

    // random eviction for now TODO do LRU
    if (is_ocs_replacement) {
      return Status::BAD; // this cache doesn't use an ocs!
    } else {              // backing store replacement
      int idx_to_evict = random() % backing_store_cache_size;
      // this is bad codestyle (repeating code) but idc
      if (cached_backing_store_pools.size() > idx_to_evict &&
          cached_backing_store_pools[idx_to_evict] != nullptr) {
        cached_backing_store_pools[idx_to_evict]->in_cache = false;
      }

      cached_backing_store_pools.assign(idx_to_evict, parent_pool);
      parent_pool->in_cache = true;
    }
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
