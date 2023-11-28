#pragma once 

#include <numbers>
#include <sstream>
#include <string>

typedef struct addr_subspace {
  // TODO should support a set of intervals
  uintptr_t addr_start; // TODO should prob use addr_t to support 32bit addresses
  uintptr_t addr_end;
  friend std::ostream& operator<<(std::ostream& os, const addr_subspace& e);
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

  friend std::ostream& operator<<(std::ostream& os, const candidate_cluster& e);
} candidate_cluster;

typedef struct pool_entry {

  // The (virtual) address range offloaded to this node.
  addr_subspace range;

  // Wether this node contains offloaded data
  bool valid = false;

  // Wether this node is 'in cache' (pointed to by OCS)
  bool in_cache = false;
  friend std::ostream& operator<<(std::ostream& os, const pool_entry& e);
} pool_entry;



