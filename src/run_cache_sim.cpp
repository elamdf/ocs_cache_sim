#include "ocs_cache_sim/lib/conservative_ocs_cache.h"
#include "ocs_cache_sim/lib/liberal_ocs_cache.h"
#include "ocs_cache_sim/lib/clock_eviction_ocs_cache.h"
#include "ocs_cache_sim/lib/clock_eviction_far_mem_cache.h"
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
  int sim_first_n_lines = options.getSimFirstNumLines();
  std::string results_filename = options.getOutputFile();
  bool verbose_output = options.enableVerboseOutput();

  std::cerr << "Loading trace from " << trace_fpath << "...\n";

  std::ifstream file(trace_fpath);
  if (!file.is_open()) {
    std::cerr << "Error opening file" << std::endl;
    return 1;
  }

  if (sim_first_n_lines > n_lines || sim_first_n_lines == -1) {
    sim_first_n_lines = n_lines;
    std::cerr << "Target samples too big... max to file size" << "\n";	
  }

  // num_pools do nothing for now
  OCSCache *random_cons_ocs_cache = new ConservativeOCSCache(
      /*num_pools=*/100, /*pool_size_bytes=*/8192, /*max_concurrent_pools=*/2,
      /*max_conrreutn_backing_store_nodes*/ 4);
  OCSCache *random_lib_ocs_cache = new LiberalOCSCache(
      /*num_pools=*/100, /*pool_size_bytes=*/8192, /*max_concurrent_pools=*/2,
      /*max_conrreutn_backing_store_nodes*/ 4);
  OCSCache *clock_ocs_cache = new ClockOCSCache(
      /*num_pools=*/100, /*pool_size_bytes=*/8192, /*max_concurrent_pools=*/2,
      /*max_conrreutn_backing_store_nodes*/ 4);
  OCSCache *farmem_cache = new FarMemCache(
      /*backing_store_cache_size*/ 4);
  OCSCache *clock_farmem_cache = new ClockFMCache(
      /*backing_store_cache_size*/ 4);

  std::vector<OCSCache *> candidates;
  candidates.push_back(random_cons_ocs_cache);
  candidates.push_back(random_lib_ocs_cache);
  candidates.push_back(clock_ocs_cache);
  candidates.push_back(farmem_cache);
  candidates.push_back(clock_farmem_cache);

  for (auto candidate : candidates) {
    std::cout << std::endl
              << "Evaluating candidate: " << candidate->getName() << std::endl;
    if (simulateTrace(file, n_lines, sim_first_n_lines, candidate,
                      /*summarize_perf=*/!verbose_output) !=
        OCSCache::Status::OK) {
      return -1;
    }
    if (verbose_output) {
      std::cout << "Final State" << std::endl;
      std::cout << *candidate;
    }
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
    std::cout << "Performance results written to " << results_filename
              << std::endl;
    out_file.close();
  }
  return 0;
}
