#pragma once

#include "constants.h"
#include "ocs_structs.h"
#include <iostream>
#include <numbers>
#include <vector>

class OCSCache {
  // We use virtual addresses here, since we're only considering
  // single-process workloads for now

public:
  enum Status { OK, BAD };

  friend std::ostream &operator<<(std::ostream &os, const OCSCache &e);

  OCSCache(int num_pools, int pool_size_bytes, int max_concurrent_ocs_pools,
           int backing_store_cache_size);
  ~OCSCache();

  // Update online clustering algorithm, potentially creating a new cluster.
  [[nodiscard]] virtual Status
  updateClustering(mem_access access, bool is_clustering_candidate) = 0;

  // Run the cache replacement algorithm for an address not present in a cached
  // memory pool or local DRAM.
  [[nodiscard]] virtual Status runOCSReplacement(mem_access access) = 0;

  // Run the cache replacement algorithm for an address not present in a cached
  // memory pool or local DRAM.
  [[nodiscard]] virtual Status
  runBackingStoreReplacement(mem_access access) = 0;

  // Handle a memory access issued by the compute node. This will update
  // clustering if relevant, and replace a cache line if neccessary.
  [[nodiscard]] Status handleMemoryAccess(mem_access access, bool *hit);

  perf_stats getPerformanceStats(bool summary);

  virtual std::string getName() const { return name; };

  void setName(const std::string &new_name) { name = new_name; };

protected:
  // Get set `node` to poitn at the pool node that services the address `addr`,
  // if it exists. Otherwise, `*node == nullptr`
  [[nodiscard]] OCSCache::Status getPoolNode(mem_access access,
                                             pool_entry **node);

  // Return if the given `addr` is serviced by `range`.
  bool accessInRange(addr_subspace &range, mem_access access);

  // Return if the given `candidate` is eligible to be materialized (turned into
  // a `pool_entry` entry).
  virtual bool
  eligibleForMaterialization(const candidate_cluster &candidate) = 0;

  // Materialize a `candidate` if it's eligible, and remove the `candidate` from
  // candidacy.
  [[nodiscard]] virtual Status
  materializeIfEligible(candidate_cluster *candidate);

  // Returns if an address will always be in DRAM (for now, this is if it's a
  // stack address).
  bool addrAlwaysInDRAM(mem_access access) {
    return access.addr > STACK_FLOOR && access.size < pool_size_bytes;
  }

  // Returns if a given `node` is in the OCS cache.
  [[nodiscard]] Status poolNodeInCache(const pool_entry &node, bool *in_cache);

  // If the address `addr` is in a cached memory pool, or local DRAM.
  [[nodiscard]] Status backingStoreNodeInCache(mem_access access,
                                               bool *in_cache);

  // If the address `addr` is in a cached memory pool, or local DRAM.
  [[nodiscard]] Status accessInCacheOrDram(mem_access access, pool_entry **node,
                                           bool *in_cache);

  // Return the candidate cluster if it exists, otherwise create one and return
  // that.
  // TODO decidde the bounds of creation. should they be an interval with `addr`
  // in the middle? safe is prob have `begin` at `addr` and `end` at `addr +
  // pool_size_bytes`
  [[nodiscard]] Status getOrCreateCandidate(mem_access access,
                                            candidate_cluster **candidate);

  // Choosing the bounds for a new candidate is a design decision.
  [[nodiscard]] virtual Status
  createCandidate(mem_access access_t, candidate_cluster **candidate) = 0;

  // Create a pool entry from a candidate cluster
  [[nodiscard]] Status
  createPoolFromCandidate(const candidate_cluster &candidate, pool_entry **pool,
                          bool is_ocs_node);

  // Return a candidate cluster if it exists, otherwise `*candidate == nullptr`
  [[nodiscard]] Status getCandidateIfExists(mem_access access,
                                            candidate_cluster **candidate);

  int num_ocs_pools; // TODO do something with this
  int num_backing_store_pools;
  int pool_size_bytes;

  std::vector<pool_entry *> cached_ocs_pools;

  // this is probably traditional, networked-backed far memory
  std::vector<pool_entry *> cached_backing_store_pools;
  std::vector<candidate_cluster *> candidates;

  // contains all (ocs + backing store) pools
  std::vector<pool_entry *> pools;

  perf_stats stats;

  // The number of pools we can concurrently point to is our 'cache' size.
  // Note that the 'cache' state is just the OCS configuation state, caching
  // data on a pool node does not physically move it.
  int ocs_cache_size;

  int backing_store_cache_size;

  std::string name;
};
