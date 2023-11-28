#pragma once 

#include <numbers>
#include <vector>
// TODO enable warn_unused_result in cflags
#include "ocs_structs.h"

// TODO make this a real value lol
#define STACK_FLOOR 0x400000000

#define RETURN_IF_ERROR(expr)                                                  \
  if (expr != Status::OK) {                                                    \
    return expr;                                                               \
  }
class OCSCache {
  // TODO we use virtual addresses here since we're only considering
  // single-process workloads for now

public:
  enum Status { OK, BAD };

  friend std::ostream& operator<<(std::ostream& os, const OCSCache& e);

  OCSCache(int num_pools, int pool_size_bytes, int cache_size);
  ~OCSCache();

  // Update online clustering algorithm, potentially creating a new cluster.
  virtual Status updateClustering(uint64_t addr,
                                  bool is_clustering_candidate) = 0;

  // Run the cache replacement algorithm for an address not present in a cached
  // memory pool or local DRAM.
  virtual Status runReplacement(uint64_t addr) = 0;

  // Handle a memory access issued by the compute node. This will update
  // clustering if relevant, and replace a cache line if neccessary.
  Status handleMemoryAccess(uint64_t addr, bool *hit);

protected:
  // Get set `node` to poitn at the pool node that services the address `addr`,
  // if it exists. Otherwise, `*node == nullptr`
  OCSCache::Status getPoolNode(uint64_t addr, pool_entry **node);

  // Return if the given `addr` is serviced by `range`.
  bool addrInRange(addr_subspace &range, uint64_t addr);

  // Return if the given `candidate` is eligible to be materialized (turned into
  // a `pool_entry` entry).
  virtual bool
  eligibleForMaterialization(const candidate_cluster &candidate) = 0;

  // Materialize a `candidate` if it's eligible, and remove the `candidate` from
  // candidacy.
  virtual Status materializeIfEligible(candidate_cluster *candidate) = 0;

  // Returns if an address will always be in DRAM (for now, this is if it's a
  // stack address).
  bool addrAlwaysInDRAM(uint64_t addr) { return addr > STACK_FLOOR; }

  // Returns if a given `node` is in the cache.
  Status poolNodeInCache(const pool_entry &node, bool *in_cache);

  // If the address `addr` is in a cached memory pool, or local DRAM.
  Status addrInCacheOrDram(uint64_t addr, bool *in_cache) {
    pool_entry *node = nullptr;
    RETURN_IF_ERROR(getPoolNode(addr, &node));
    if (node == nullptr) {
      *in_cache = false;
    } else {
      RETURN_IF_ERROR(poolNodeInCache(*node, in_cache));
    }

    *in_cache = *in_cache || addrAlwaysInDRAM(addr);

    return Status::OK;
  }

  // Return the candidate cluster if it exists, otherwise create one and return
  // that.
  // TODO decidde the bounds of creation. should they be an interval with `addr`
  // in the middle? safe is prob have `begin` at `addr` and `end` at `addr +
  // pool_size_bytes`
  Status getOrCreateCandidate(uint64_t addr, candidate_cluster **candidate);

  // Choosing the bounds for a new candidate is a design decision.
  virtual Status createCandidate(uint64_t addr_t,
                                 candidate_cluster **candidate) = 0;

  // Return a candidate cluster if it exists, otherwise `*candidate == nullptr`
  Status getCandidateIfExists(uint64_t addr, candidate_cluster **candidate);


  int num_pools;
  int pool_size_bytes;
  std::vector<pool_entry *> cached_pools;
  std::vector<candidate_cluster *> candidates;
  std::vector<pool_entry *> pools;

  // The number of pools we can concurrently point to is our 'cache' size.
  // Note that the 'cache' state is just the OCS configuation state, caching
  // data on a pool node does not physically move it.
  int cache_size;
};
