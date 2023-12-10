#include "utils.h"
#include "ocs_cache_sim/lib/ocs_structs.h"
#include <iostream>
#include <fstream>

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


[[nodiscard]] OCSCache::Status simulateTrace(std::ifstream &trace, int n_lines,
                                             OCSCache *cache) {
  std::string line;
  // Reading the header line
  int header_lines = 2;
  int line_number = 0;

  if (n_lines != -1) {
    std::cout << "trace has " << n_lines - header_lines << " accesses"
              << std::endl;
  } else {
    std::cout << "trace has an unkown number of accesses" << std::endl;
  }
  std::cerr << "Simulating Trace...\n";
  // Reading each line of the file
  while (getline(trace, line)) {
    if (header_lines > 0) {
      header_lines--;
      continue;
    }

    std::istringstream ss(line);
    std::string token;
    std::vector<std::string> columns;

    // Splitting line into columns
    while (getline(ss, token, ',')) {
      columns.push_back(token);
    }

    // long timestamp = stol(columns[1]);
    uintptr_t address = stol(columns[1]);
    int size = stoi(columns[2]);
    // std::string type = columns[3];

    bool hit = false;
    if (cache->handleMemoryAccess({address, size}, &hit) !=
        OCSCache::Status::OK) {
      std::cout << "handleMemoryAccess failed somewhere :(\n";
      return OCSCache::Status::BAD;
    }

    if (!DEBUG && n_lines > 0) { // TODO this shouldn't be done every line
      printProgress(static_cast<double>(line_number) / n_lines);
    }

    line_number++;
  }

  std::cerr << std::endl << "Simulation complete!" << std::endl;
  // std::cout << *cache;
  std::cerr << cache->getPerformanceStats(/*summary=*/true);
  return OCSCache::Status::OK;
}
