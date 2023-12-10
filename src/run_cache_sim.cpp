#include "ocs_cache_sim/lib/basic_ocs_cache.h"
#include "ocs_cache_sim/lib/far_memory_cache.h"
#include "ocs_cache_sim/lib/ocs_cache.h"
#include "ocs_cache_sim/lib/ocs_structs.h"
#include "ocs_cache_sim/lib/utils.h"
#include "ocs_cache_sim/src/CLIOpts.h"

#include <boost/program_options.hpp>
#include <fstream>
#include <iostream>
namespace po = boost::program_options;

int main(int argc, char *argv[]) {

  CLIOpts options;
  options.parse(argc, argv);

  std::string trace_fpath = options.getInputFile();
  int n_lines = options.getNumLines();
  std::string results_filename = options.getOutputFile();

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
  candidates.push_back(ocs_cache);
  candidates.push_back(farmem_cache);

  for (auto candidate : candidates) {
    std::cout << std::endl
              << "Evaluating candidate: " << candidate->getName() << std::endl;
    if (simulateTrace(file, n_lines, candidate) != OCSCache::Status::OK) {
      return -1;
    }
    std::cout << *candidate;
    file.clear();
    file.seekg(0);
  }

  file.close();

  // write results if provided an output fname
  if (results_filename.length() > 0) {
    std::ofstream out_file(results_filename, std::ios::out | std::ios::trunc);

    if (writePerfSummary(candidates, trace_fpath, out_file) !=
        OCSCache::Status::OK) {
      return -1;
    }
    std::cout << "Performance results written to " << results_filename << std::endl;
    out_file.close();
  }
  return 0;
}
