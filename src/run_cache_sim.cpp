#include "ocs_cache_sim/lib/basic_ocs_cache.h"
#include "ocs_cache_sim/lib/far_memory_cache.h"
#include "ocs_cache_sim/lib/ocs_cache.h"
#include "ocs_cache_sim/lib/ocs_structs.h"
#include "ocs_cache_sim/lib/utils.h"
#include <fstream>
#include <iostream>

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
