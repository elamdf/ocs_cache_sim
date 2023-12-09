#pragma once

#include "ocs_structs.h"
#include <vector>

// Returns the ranges touched by `access` that aren't covered by `pages`
std::vector<addr_subspace> findUncoveredRanges(const mem_access & access, std::vector<pool_entry *> *pages);
