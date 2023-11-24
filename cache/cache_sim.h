#include <numbers>
#include <vector>
// TODO enable warn_unused_result in cflags
// also write a makefile lol

// TODO make this a real value lol
#define STACK_FLOOR 0x400000000

#define RETURN_IF_ERROR(expr)                                                  \
  if (expr != Status::OK) {                                                    \
    return expr;                                                               \
  }
class OCS {
  // TODO we use virtual addresses here since we're only considering
  // single-process workloads for now

  enum Status { OK, BAD };

public:
  OCS(int num_pools, int pool_size_bytes, int cache_size);

  // Update online clustering algorithm, potentially creating a new cluster.
  Status updateClustering(uint64_t addr, bool is_clustering_candidate);

  // Run the cache replacement algorithm for an address not present in a cached
  // memory pool or local DRAM.
  Status runReplacement(uint64_t addr);

  // Handle a memory access issued by the compute node. This will update
  // clustering if relevant, and replace a cache line if neccessary.
  Status handleMemoryAccess(uint64_t addr, bool *hit);

protected:
  // TODO move these to where struct defs go i forgor
  typedef struct addr_subspace {
    // TODO should support a set of intervals
    uint64_t
        addr_start; // TODO should prob use addr_t to support 32bit addresses
    uint64_t addr_end;
  } addr_subspace;

  typedef struct candidate_cluster {
    addr_subspace range;

    // the number of accesses, since the creation of this cluster, within
    // `range`
    int on_cluster_accesses;

    // the number of accesses, since the creation of this cluster, without
    // `range` (not including local DRAM (i.e. stack) accesses)
    int off_cluster_accesses;
    bool valid;

  } candidate_cluster;

  typedef struct pool_node {

    // The (virtual) address range offloaded to this node.
    addr_subspace range;

    // Wether this node contains offloaded data
    bool valid = false;

    // Wether this node is 'in cache' (pointed to by OCS)
    bool in_cache = false;
  } pool_entry;

  // Get set `node` to poitn at the pool node that services the address `addr`,
  // if it exists. Otherwise, `node == nullptr`
  OCS::Status getPoolNode(uint64_t addr, pool_node *node);

  // Return if the given `addr` is serviced by `range`.
  bool addrInRange(addr_subspace &range, uint64_t addr);

  // Return if the given `candidate` is eligible to be materialized (turned into
  // a `pool_node` entry).
  bool eligibleForMaterialization(candidate_cluster &candidate);

  // Materialize a `candidate` if it's eligible, and remove the `candidate` from
  // candidacy.
  void materializeIfEligible(candidate_cluster &candidate);

  // Returns if an address will always be in DRAM (for now, this is if it's a
  // stack address).
  bool addrAlwaysInDRAM(uint64_t addr) { return addr > STACK_FLOOR; }

  // Returns if a given `node` is in the cache.
  Status poolNodeInCache(const pool_node &node, bool *in_cache);

  // If the address `addr` is in a cached memory pool, or local DRAM.
  Status addrInCacheOrDram(uint64_t addr, bool *in_cache) {
    pool_node *node;

    RETURN_IF_ERROR(getPoolNode(addr, node));
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
  candidate_cluster &getOrCreateCandidate(uint64_t addr);

  // Return a candidate cluster if it exists, otherwise return an object with
  // `valid == false`.
  candidate_cluster &getCandidateIfExists(uint64_t addr);

  int num_pools;
  int pool_size_bytes;
  std::vector<pool_entry *> cached_pools;
  std::vector<pool_entry> pools;

  // The number of pools we can concurrently point to is our 'cache' size.
  // Note that the 'cache' state is just the OCS configuation state, caching
  // data on a pool node does not physically move it.
  int cache_size;
};
