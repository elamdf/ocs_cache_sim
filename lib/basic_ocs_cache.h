#pragma once

#include "ocs_cache.h"
#include "ocs_structs.h"
#include <algorithm>
class BasicOCSCache : public OCSCache {

public:
  BasicOCSCache(int num_pools, int pool_size_bytes,
                int max_concurrent_ocs_pools, int backing_store_cache_size)
      : OCSCache(num_pools, pool_size_bytes, max_concurrent_ocs_pools,
                 backing_store_cache_size) {}

protected:
  [[nodiscard]] Status
  updateClustering(mem_access access, bool is_clustering_candidate) {
    candidate_cluster *candidate = nullptr;
    RETURN_IF_ERROR(is_clustering_candidate
                        ? getOrCreateCandidate(access, &candidate)
                        : getCandidateIfExists(access, &candidate));
    if (candidate != nullptr) {
      // TODO this will never increment off cluster accesses, we need to figure
      // out how to switch to a new candidate have one 'promotable' candidate at
      // once, have some criteria to switch that (ie some number of contiguous
      // off-cluster accesses)
      if (accessInRange(candidate->range, access)) {
        candidate->on_cluster_accesses++;
      } else {
        candidate->off_cluster_accesses++;
        // TODO we need some way to remove candidates if they suck, otherwise we
        // will always short circuit on them.
        if (candidate->off_cluster_accesses > 10) {
        }
      }
      // TODO should we update the bounds of a candidate based on more complex
      // access insights?
      RETURN_IF_ERROR(materializeIfEligible(candidate));
    }
    return Status::OK;
  }

  [[nodiscard]] Status createCandidate(mem_access access,
                                       candidate_cluster **candidate) {
    *candidate = (candidate_cluster *)malloc(sizeof(candidate_cluster));
    if (*candidate == nullptr) {
      return Status::BAD;
    }

    // naive strategy, this has bad countexamples when first access is the
    // minimum, as it usually(?) is
    addr_subspace s;
    if (s.addr_start < .25 * pool_size_bytes) {
      s.addr_start = access.addr - .25 * pool_size_bytes;
      s.addr_end = access.addr + .75 * pool_size_bytes;
    } else { // weird edge case
      s.addr_start = access.addr;
      s.addr_end = access.addr + pool_size_bytes;
    }

    (*candidate)->range = s;
    (*candidate)->id = candidates.size();
    (*candidate)->on_cluster_accesses = 0;
    (*candidate)->off_cluster_accesses = 0;
    (*candidate)->valid = true;

    DEBUG_LOG("creating candidate with address range "
              << s.addr_start << ":" << s.addr_end << std::endl);

    stats.candidates_created++;
    return Status::OK;
  }

  // basically random for now
  bool eligibleForMaterialization(const candidate_cluster &candidate) {
    return candidate.valid && candidate.on_cluster_accesses > 100 &&
           candidate.on_cluster_accesses > 10 * candidate.off_cluster_accesses;
  }

  candidate_cluster *current_candidate;

  std::string getName() const { return "Basic OCS cache with random replacement for both NFM and backing store"; }
};
