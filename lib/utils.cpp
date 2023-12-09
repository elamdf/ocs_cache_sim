#include "utils.h"
#include "ocs_cache_sim/lib/ocs_structs.h"
#include <iostream>

std::vector<addr_subspace> findUncoveredRanges(const mem_access & access, std::vector<pool_entry *> *pages) {
    std::vector<addr_subspace> uncoveredRanges;
    uintptr_t currentAddress = access.addr;
    uintptr_t endAddress = access.addr + access.size;

    while (currentAddress < endAddress) {
        bool uncovered = true;

        for (const auto& page : *pages) {
            if (page->range.addr_start<= currentAddress && currentAddress < page->range.addr_end) {
                currentAddress = page->range.addr_end;
                uncovered = false;
                break;
            }
        }

        if (uncovered) {
            uintptr_t nextCovered = endAddress;
            for (const auto& page : *pages) {
                if (page->range.addr_start > currentAddress) {
                    nextCovered = std::min(nextCovered, page->range.addr_start);
                }
            }
            addr_subspace sp = {currentAddress, nextCovered};
            uncoveredRanges.emplace_back(sp);
        std::cout << "uncovered range from " << access << ": " << sp << std::endl;
            currentAddress = nextCovered;
        }
    }

    return uncoveredRanges;
}

