#pragma once

#include "ocs_cache.h"
#include "ocs_structs.h"
#include <vector>

// Returns the ranges touched by `access` that aren't covered by `pages`
std::vector<addr_subspace> findUncoveredRanges(const mem_access & access, std::vector<pool_entry *> *pages);

[[nodiscard]] OCSCache::Status simulateTrace(std::ifstream &trace, int n_lines,
                                             OCSCache *cache);
