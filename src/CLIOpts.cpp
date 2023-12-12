#include "ocs_cache_sim/src/CLIOpts.h"
#include <boost/program_options.hpp>
#include <iostream>

namespace po = boost::program_options;

CLIOpts::CLIOpts()
    : desc("Allowed options"), num_lines(0), outputFile("test.csv") {
  // Define command line options
  desc.add_options()("input_file",
                     po::value<std::string>(&inputFile)->required(),
                     "The trace file to be simulated")(
      "num_lines,n", po::value<int>(&num_lines)->default_value(-1),
      "The number of lines in the trace file, so a progress bar can be "
      "displayed during simulation")(
	  "sample_num_lines", po::value<int>(&sample_num_lines)->default_value(-1),
	  "The number of lines to be sampled in the trace file")(
      "output_file,o", po::value<std::string>(&outputFile)->default_value(""),
      "The filename to write results to, if desired")
      ("display_full_results,v", po::bool_switch(&verbose),
       "Enable verbose output"); // Boolean switch
}

void CLIOpts::parse(int argc, char *argv[]) {
  // Define positional options
  po::positional_options_description p;
  p.add("input_file", 1); // One positional argument for input_file
  p.add("num_lines", 1);  // One optional positional argument for num_lines

  try {
    po::store(
        po::command_line_parser(argc, argv).options(desc).positional(p).run(),
        vm);
    po::notify(vm);

    if (vm.count("input_file")) {
      inputFile = vm["input_file"].as<std::string>();
    }
    if (vm.count("num_lines")) {
      num_lines = vm["num_lines"].as<int>();
    }
    if (vm.count("sample_num_lines")) {
      sample_num_lines = vm["sample_num_lines"].as<int>();
    }
    if (vm.count("output_file")) {
      outputFile = vm["output_file"].as<std::string>();
    }
    if (vm.count("display_full_results")) {
      verbose = vm["display_full_results"].as<bool>();
    }
  } catch (const po::error &ex) {
    std::cerr << ex.what() << std::endl;
    std::cerr << desc << std::endl;
    exit(EXIT_FAILURE);
  }
}

std::string CLIOpts::getInputFile() const { return inputFile; }

int CLIOpts::getNumLines() const { return num_lines; }
int CLIOpts::getSampleNumLines() const { return sample_num_lines; }
bool CLIOpts::enableVerboseOutput() const { return verbose; }

std::string CLIOpts::getOutputFile() const { return outputFile; }
