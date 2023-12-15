#include "ocs_cache_sim/lib/clock_eviction_far_mem_cache.h"
#include "ocs_cache_sim/lib/clock_eviction_ocs_cache.h"
#include "ocs_cache_sim/lib/conservative_clock_ocs_cache.h"
#include "ocs_cache_sim/lib/conservative_random_ocs_cache.h"
#include "ocs_cache_sim/lib/far_memory_cache.h"
#include "ocs_cache_sim/lib/liberal_clock_ocs_cache.h"
#include "ocs_cache_sim/lib/liberal_random_ocs_cache.h"
#include "ocs_cache_sim/lib/ocs_cache.h"
#include "ocs_cache_sim/lib/ocs_structs.h"
#include "ocs_cache_sim/lib/utils.h"
#include "ocs_cache_sim/src/CLIOpts.h"

#include <boost/program_options.hpp>
#include <fstream>
#include <future>
#include <iostream>
#include <thread>

int main(int argc, char *argv[]) {

  CLIOpts options;
  options.parse(argc, argv);

  std::string trace_fpath = options.getInputFile();
  int n_lines = options.getNumLines();
  int sim_first_n_lines = options.getSimFirstNumLines();
  std::string results_filename = options.getOutputFile();
  bool verbose_output = options.enableVerboseOutput();

  std::cerr << "Loading trace from " << trace_fpath << "...\n";

  if (sim_first_n_lines > n_lines || sim_first_n_lines == -1) {
    sim_first_n_lines = n_lines;
    if (sim_first_n_lines > n_lines) {
      std::cout << "sim_first_n_lines > n_lines! ";
    }
    std::cout << "Simulating entire file"
              << "\n";
  } else {
    std::cout << "Simulating first " << sim_first_n_lines << " lines"
              << "\n";
  }

  std::vector<OCSCache *> candidates;

  // Conservative Clustering
  OCSCache *cons_random_ocs = new ConservativeRandomOCSCache(
      /*pool_size_bytes=*/8192, /*max_concurrent_pools=*/2,
      /*max_conrreutn_backing_store_nodes*/ 4);
  candidates.push_back(cons_random_ocs);
  OCSCache *lib_random_ocs = new LiberalRandomOCSCache(
      /*pool_size_bytes=*/8192, /*max_concurrent_pools=*/2,
      /*max_conrreutn_backing_store_nodes*/ 4);
  candidates.push_back(lib_random_ocs);

  // Liberal Clustering
  OCSCache *cons_clock_ocs = new ConservativeClockOCSCache(
      /*pool_size_bytes=*/8192, /*max_concurrent_pools=*/2,
      /*max_conrreutn_backing_store_nodes*/ 4);
  candidates.push_back(cons_clock_ocs);
  OCSCache *lib_clock_ocs = new LiberalClockOCSCache(
      /*pool_size_bytes=*/8192, /*max_concurrent_pools=*/2,
      /*max_conrreutn_backing_store_nodes*/ 4);
  candidates.push_back(lib_clock_ocs);

  // Control: No OCS
  OCSCache *farmem_cache_random = new FarMemCache(
      /*backing_store_cache_size*/ 4);
  candidates.push_back(farmem_cache_random);
  OCSCache *farmem_cache_clock = new ClockFMCache(
      /*backing_store_cache_size*/ 4);
  candidates.push_back(farmem_cache_clock);

  std::vector<std::future<OCSCache::Status>> futures;

  for (auto candidate : candidates) {
    std::cout << std::endl
              << "Evaluating candidate: " << candidate->getName() << std::endl;
    if (ENABLE_MULTITHREADING) {
      std::ifstream new_fd(trace_fpath);
      futures.push_back(std::async(std::launch::async,simulateTrace, std::cref(trace_fpath), n_lines,
                                    sim_first_n_lines, candidate,
                                    /*summarize_perf=*/!verbose_output));
    } else {
      if (simulateTrace(trace_fpath, n_lines, sim_first_n_lines, candidate,
                        /*summarize_perf=*/!verbose_output) !=
          OCSCache::Status::OK) {
        return -1;
      }
    }
    if (!ENABLE_MULTITHREADING && verbose_output) {
      std::cout << "Final State" << std::endl;
      std::cout << *candidate;
    }
  }

  if (ENABLE_MULTITHREADING) {
    for (auto &th : futures) {
      if (th.get() != OCSCache::Status::OK) {
        return -1;
      }
    }
  }


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
