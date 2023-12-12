#include "utils.h"
#include "ocs_cache_sim/lib/ocs_structs.h"
#include <fstream>
#include <iostream>

std::vector<addr_subspace>
findUncoveredRanges(const mem_access &access,
                    std::vector<pool_entry *> *pages) {
  std::vector<addr_subspace> uncoveredRanges;
  uintptr_t currentAddress = access.addr;
  uintptr_t endAddress = access.addr + access.size;

  while (currentAddress < endAddress) {
    bool uncovered = true;

    for (const auto &page : *pages) {
      if (page->range.addr_start <= currentAddress &&
          currentAddress < page->range.addr_end) {
        currentAddress = page->range.addr_end;
        uncovered = false;
        break;
      }
    }

    if (uncovered) {
      uintptr_t nextCovered = endAddress;
      for (const auto &page : *pages) {
        if (page->range.addr_start > currentAddress) {
          nextCovered = std::min(nextCovered, page->range.addr_start);
        }
      }
      addr_subspace sp = {currentAddress, nextCovered};
      uncoveredRanges.emplace_back(sp);
      DEBUG_LOG("uncovered range from " << access << ": " << sp << std::endl);
      currentAddress = nextCovered;
    }
  }

  return uncoveredRanges;
}

[[nodiscard]] OCSCache::Status simulateTrace(std::ifstream &trace, int n_lines, int sim_first_n_lines,
                                             OCSCache *cache, bool summarize_perf) {
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
	if (line_number >= sim_first_n_lines && sim_first_n_lines > 0) {
      break;
	}
  }

  std::cerr << std::endl << "Simulation complete!" << std::endl;
  std::cerr << cache->getPerformanceStats(/*summary=*/summarize_perf);
  return OCSCache::Status::OK;
}

OCSCache::Status writePerfSummary(std::vector<OCSCache *> caches,
                                  const std::string &trace_filename,
                                  std::ofstream &results_file) {
  if (!results_file.is_open()) {
    std::cerr << "error opening results file " << std::endl;
    return OCSCache::Status::OK;
  }
  results_file
      << "Cache Name, Trace File, Total Off-Node Memory Usage, Total Accesses, "
         "DRAM Accesses, Overall "
         "latency, #NFM nodes, #Backing Store nodes, NFM Utilization, "
         "Backing Store Utilization, NFM Hit Rate, Backing Store Hit "
         "Rate, NFM hits, Backing Store hits, NFM Misses, Backing Store "
         "Misses, Cluster Candidates Created, Candidate Promotion Rate"
      << std::endl;
  for (OCSCache *cache : caches) {
    perf_stats stats = cache->getPerformanceStats();
    long backing_store_accesses =
        (stats.backing_store_hits + stats.backing_store_misses);
    long ocs_accesses = (stats.ocs_pool_hits + stats.ocs_reconfigurations);

    double ocs_hit_rate =
        ocs_accesses > 0
            ? static_cast<double>(stats.ocs_pool_hits) / ocs_accesses
            : 0.0;

    double ocs_utilization = static_cast<double>(ocs_accesses) / stats.accesses;
    double backing_store_utilization =
        static_cast<double>(backing_store_accesses) / stats.accesses;

    double backing_store_hit_rate =
        backing_store_accesses > 0
            ? static_cast<double>(stats.backing_store_hits) /
                  backing_store_accesses
            : 0.0;

    double promotion_rate =
        stats.candidates_created > 0
            ? static_cast<double>(stats.candidates_promoted) /
                  stats.candidates_created
            : 0.0;
    long total_off_node_mem_usage =
        stats.ocs_pool_mem_usage + stats.backing_store_mem_usage;

    // TODO there has to be a better way
    results_file << cache->getName() << "," << trace_filename << ","
                 << total_off_node_mem_usage << "," << stats.accesses << ","
                 << stats.dram_hits << ","
                 << "TODO"
                 << "," << stats.num_ocs_pools << ","
                 << stats.num_backing_store_pools << "," << ocs_utilization
                 << "," << backing_store_utilization << "," << ocs_hit_rate
                 << "," << backing_store_hit_rate << "," << stats.ocs_pool_hits
                 << "," << stats.backing_store_hits << ","
                 << stats.ocs_reconfigurations << ","
                 << stats.backing_store_misses << ","
                 << stats.candidates_created << "," << promotion_rate
                 << std::endl;
  }

  return OCSCache::Status::OK;
}
