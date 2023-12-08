#include "basic_ocs_cache.h"
#include "far_memory_cache.h"
#include "ocs_cache.h"
#include "ocs_structs.h"
#include <fstream>
#include <iostream>

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
        BasicOCSCache::Status::OK) {
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

int main(int argc, char *argv[]) {

  std::string trace_fpath;
  int n_lines = -1;
  if (argc > 1) {
    trace_fpath = std::string(argv[1]);

    if (argc > 2) { // second arg, if it exists, is number of lines in file
      n_lines = std::stoi(argv[2]);
    }
  } else {
    trace_fpath = "tests/fixture/test_ls.csv";
  }
  std::cerr << "Loading trace from " << trace_fpath << "...\n";

  std::ifstream file(trace_fpath);
  if (!file.is_open()) {
    std::cerr << "Error opening file" << std::endl;
    return 1;
  }

  OCSCache *ocs_cache = new BasicOCSCache(
      /*num_pools=*/100, /*pool_size_bytes=*/8192, /*max_concurrent_pools=*/1,
      /*max_conrreutn_backing_store_nodes*/ 100);
  OCSCache *farmem_cache = new FarMemCache(
      /*backing_store_cache_size*/ 100);

  std::vector<OCSCache *> candidates;
  candidates.push_back( ocs_cache);
  candidates.push_back( farmem_cache);

  for (auto candidate : candidates) {
    std::cout << std::endl << "Evaluating candidate: " << candidate->getName() << std::endl;
    if (simulateTrace(file, n_lines, candidate) !=
        OCSCache::Status::OK) {
      return -1;
    }
    std::cout << *candidate;
    file.clear();
    file.seekg(0);
  }

  file.close();
  return 0;
}
