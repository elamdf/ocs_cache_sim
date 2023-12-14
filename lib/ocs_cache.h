#pragma once
// TODO There should really be tests for like all of this

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

  OCSCache(int pool_size_bytes, int max_concurrent_ocs_pools,
           int backing_store_cache_size);
  ~OCSCache();

  // Update online clustering algorithm, potentially creating a new cluster.
  [[nodiscard]] virtual Status
  updateClustering(mem_access access, bool is_clustering_candidate) = 0;

  // Returns the cache index to replace. This makes implementing custom policies
  // easier to do without rewriting a bunch of replacement business logic
  // Random by default.
  [[nodiscard]] virtual size_t indexToReplace(bool is_ocs_pool);

  // Swap all `parent_pools` with `in_cache=false` in to their respective cache
  // (given by `parent_pools[i] == is_ocs_replacement[i]`. Random replacement
  // policy by default.
  [[nodiscard]] Status runReplacement(mem_access access,
                                      std::vector<pool_entry *> parent_pools);

  // Handle a memory access issued by the compute node. This will update
  // clustering if relevant, and replace a cache line if neccessary.
  [[nodiscard]] Status handleMemoryAccess(mem_access access, bool *hit);

  perf_stats getPerformanceStats();

  perf_stats getPerformanceStats(bool summary);

  virtual std::string getName() const = 0;

protected:
  // Get the nodes that, together, contain `access`. `parent_nodes.size() == 0`
  // if there is no associated
  [[nodiscard]] OCSCache::Status
  getPoolNodes(mem_access access, std::vector<pool_entry *> *parent_nodes);

  // Return if the given `addr` is serviced by `range`. Note that an access
  // can span multiple ranges
  bool accessInRange(addr_subspace &range, mem_access access);

  // Return if the given `candidate` is eligible to be materialized (turned
  // into a `pool_entry` entry).
  virtual bool
  eligibleForMaterialization(const candidate_cluster &candidate) = 0;

  // Materialize a `candidate` if it's eligible, and remove the `candidate`
  // from candidacy.
  [[nodiscard]] virtual Status
  materializeIfEligible(candidate_cluster *candidate);

  // Returns if an address will always be in DRAM (for now, this is if it's a
  // stack address).
  bool addrAlwaysInDRAM(mem_access access) {
    return access.addr > STACK_FLOOR; //&& access.size < pool_size_bytes; causes problems
  }

  // Returns if a given `node` is in the OCS cache.
  [[nodiscard]] Status poolNodesInCache(std::vector<pool_entry *> *nodes,
                                        std::vector<bool> *in_cache);

  // If the address `addr` is in a cached memory pool, or local DRAM.
  [[nodiscard]] Status backingStoreNodeInCache(mem_access access,
                                               bool *in_cache);

  // If the access `access` is in a cached memory pool, or local DRAM.
  [[nodiscard]] Status
  accessInCacheOrDram(mem_access access,
                      std::vector<pool_entry *> *associated_nodes,
                      std::vector<bool> *in_cache, bool *dram_hit);

  // Return the candidate cluster if it exists, otherwise create one and
  // return that.
  [[nodiscard]] Status getOrCreateCandidate(mem_access access,
                                            candidate_cluster **candidate);

  // Choosing the bounds for a new candidate is a design decision.
  [[nodiscard]] virtual Status
  createCandidate(mem_access access_t, candidate_cluster **candidate) = 0;

  // Create a pool entry from a candidate cluster
  [[nodiscard]] Status
  createPoolFromCandidate(const candidate_cluster &candidate, pool_entry **pool,
                          bool is_ocs_node);

  // Return a candidate cluster if it exists, otherwise `*candidate ==
  // nullptr`
  [[nodiscard]] Status getCandidateIfExists(mem_access access,
                                            candidate_cluster **candidate);

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
  int max_ocs_cache_size;

  int max_backing_store_cache_size;
};
