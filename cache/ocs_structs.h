#include <numbers>

typedef struct addr_subspace {
  // TODO should support a set of intervals
  uint64_t addr_start; // TODO should prob use addr_t to support 32bit addresses
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
