#pragma once

#include "basic_ocs_cache.h"

class ClockOCSCache : public BasicOCSCache {
public:
  ClockOCSCache( int pool_size_bytes,
                int max_concurrent_ocs_pools, int backing_store_cache_size)
      : BasicOCSCache( pool_size_bytes, max_concurrent_ocs_pools,
                      backing_store_cache_size) {}

  // for testing
  int getClockHand(bool ocs_cache) const {
    return ocs_cache ? ocs_clock_hand : backing_store_clock_hand;
  }
  std::vector<bool> getRefBits(bool ocs_cache) const {
    return ocs_cache ? ocs_referenced_bits : backing_store_referenced_bits;
  }

  [[nodiscard]] Status handleMemoryAccess(mem_access access, bool *hit) {
      RETURN_IF_ERROR(BasicOCSCache::handleMemoryAccess(access, hit));

      for (bool b : backing_store_referenced_bits) {
          std::cout << b << " ";
      }
      std::cout << std::endl;
      return Status::OK;

  }
protected:

  [[nodiscard]] Status poolNodesInCache(std::vector<pool_entry *> *nodes,
                                        std::vector<bool> *in_cache) {
    RETURN_IF_ERROR(BasicOCSCache::poolNodesInCache(nodes, in_cache));
    DEBUG_CHECK(nodes->size() == in_cache->size(),
                "they gotta be the same size");

    // mark referenced nodes
    for (size_t i = 0; i < in_cache->size(); i++) {
      if ((*in_cache)[i]) {
        pool_entry *node = (*nodes)[i];
        auto cache =
            node->is_ocs_pool ? &cached_ocs_pools : &cached_backing_store_pools;
        auto ref_bitvector = node->is_ocs_pool ? &ocs_referenced_bits
                                               : &backing_store_referenced_bits;

        auto it = std::find(cache->begin(), cache->end(), node);
        DEBUG_CHECK(it != cache->end(),
                    "didn't find supposedly cached node in cache");
        int node_idx = std::distance(cache->begin(), it);
        // note that we don't update the hand on hit in this instance of clock
        (*ref_bitvector)[node_idx] = true;
      }
    }
    return Status::OK;
  }

  [[nodiscard]] size_t indexToReplace(bool is_ocs_replacement) override {
    // Clock eviction
    std::vector<pool_entry *> *cache;
    std::vector<bool> *ref_bitvector;
    size_t *clock_hand;
    size_t max_cache_size;
    if (is_ocs_replacement) {
      cache = &cached_ocs_pools;
      ref_bitvector = &ocs_referenced_bits;
      clock_hand = &ocs_clock_hand;
      max_cache_size = max_ocs_cache_size;
    } else {
      cache = &cached_backing_store_pools;
      ref_bitvector = &backing_store_referenced_bits;
      clock_hand = &backing_store_clock_hand;
      max_cache_size = max_backing_store_cache_size;
    }

    if (cache->size() < max_cache_size) {
      ref_bitvector->push_back(true);
      DEBUG_LOG("[addition] indexToReplace returning " << cache->size());
      return cache->size();
    }

    while ((*ref_bitvector)[*clock_hand]) {
      (*ref_bitvector)[*clock_hand] = false;
      *clock_hand = (*clock_hand + 1) % cache->size();
    }

    size_t ret_idx = *clock_hand;
    *clock_hand = (*clock_hand + 1) % cache->size();

    (*ref_bitvector)[ret_idx] = true;

    DEBUG_LOG("[eviction] indexToReplace returning " << ret_idx);
    return ret_idx;
  }

  std::string getName() override {
    return "OCS cache with clock replacement for both NFM and backing "
           "stores";
  }

  size_t ocs_clock_hand = 0;
  size_t backing_store_clock_hand = 0;
  std::vector<bool> ocs_referenced_bits;
  std::vector<bool> backing_store_referenced_bits;
};
