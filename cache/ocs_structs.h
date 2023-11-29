#pragma once

#include <numbers>
#include <sstream>
#include <string>

typedef struct addr_subspace {
  // TODO should support a set of intervals
  uintptr_t
      addr_start; // TODO should prob use addr_t to support 32bit addresses
  uintptr_t addr_end;
  friend std::ostream &operator<<(std::ostream &os, const addr_subspace &e);
} addr_subspace;

typedef struct candidate_cluster {
  int id;
  addr_subspace range;

  // the number of accesses, since the creation of this cluster, within
  // `range`
  int on_cluster_accesses;

  // the number of accesses, since the creation of this cluster, without
  // `range` (not including local DRAM (i.e. stack) accesses)
  int off_cluster_accesses;
  bool valid;

  friend std::ostream &operator<<(std::ostream &os, const candidate_cluster &e);
  bool operator==(const candidate_cluster &A) const { return id == A.id; };
} candidate_cluster;

typedef struct pool_entry {
  int id;

  // The (virtual) address range offloaded to this node.
  addr_subspace range;

  // Wether this node contains offloaded data
  bool valid = false;

  // Wether this node is 'in cache' (pointed to by OCS)
  bool in_cache = false;
  friend std::ostream &operator<<(std::ostream &os, const pool_entry &e);
  bool operator==(const pool_entry &A) const { return id == A.id; };
} pool_entry;

typedef struct mem_access {
  uintptr_t addr;
  int size;
} mem_access;

typedef struct perf_stats {
  long accesses = 0;
  long hits = 0;
  long misses = 0;
  long candidates_created = 0;
  long candidates_promoted = 0;
  friend std::ostream &operator<<(std::ostream &os, const perf_stats &stats);

} perf_stats;
