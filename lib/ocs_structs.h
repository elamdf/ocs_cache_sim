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
  long size() const { return addr_end - addr_start; }
} addr_subspace;

typedef struct candidate_cluster {
  int id;
  addr_subspace range;

  // the number of accesses, since the creation of this cluster, within
  // `range`
  int on_cluster_accesses = 0;

  // the number of accesses, since the creation of this cluster, without
  // `range` (not including local DRAM (i.e. stack) accesses)
  int off_cluster_accesses = 0;
  bool valid = false;

  friend std::ostream &operator<<(std::ostream &os, const candidate_cluster &e);
  bool operator==(const candidate_cluster &A) const { return id == A.id; };
} candidate_cluster;

typedef struct pool_entry {
  int id;

  // The (virtual) address range offloaded to this node.
  addr_subspace range;

  // Wether this node contains offloaded data
  bool valid = false;

  // True if this is an OCS-cachable pool, false if it is a far-memory pool
  bool is_ocs_pool = false;

  // Wether this node is 'in cache' (pointed to by OCS)
  bool in_cache = false;
  friend std::ostream &operator<<(std::ostream &os, const pool_entry &e);
  bool operator==(const pool_entry &A) const { return id == A.id; };
  long size() const { return range.size(); }
} pool_entry;

typedef struct mem_access {
  uintptr_t addr;
  int size;

  friend std::ostream &operator<<(std::ostream &os, const mem_access &a);
} mem_access;

typedef struct perf_stats {
  // NOTE that this is the number of cache/node accesses, which might be greater than
  // the number of memory accesses, because memory accesses (such as those
  // bigger than a page) might lead to multiple cache accesses (misses/hits).
  long accesses = 0;
  long ocs_pool_mem_usage = 0; // updated at getPerformanceStats() call time
  long backing_store_mem_usage =
      0; // updated at getPerformanceStats() call time

  long ocs_reconfigurations = 0;
  long backing_store_misses = 0;

  // technically, accessing something like the stack is a 'hit', but it can
  // artificially inflate (what we percieve to be) our pooling performance so we
  // seperate it out
  long dram_hits = 0;
  long ocs_pool_hits = 0;
  long backing_store_hits = 0;

  long num_ocs_pools = 0; // updated at getPerformanceStats() call time
  long num_backing_store_pools =
      0; // updated at getPerformanceStats() call time
  long candidates_created = 0;
  long candidates_promoted = 0;
  friend std::ostream &operator<<(std::ostream &os, const perf_stats &stats);

  bool summary = false;
} perf_stats;
