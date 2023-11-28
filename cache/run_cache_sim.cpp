#include "basic_ocs_cache.h"
#include <fstream>
#include <iostream>

int main(int argc, char *argv[]) {

  std::string trace_fpath;
  if (argc > 1) {
    trace_fpath = std::string(argv[1]);
  } else {
    std::cout << "defaulting\n";
    trace_fpath = "tests/fixture/test_ls.csv";
  }
  std::ifstream file(trace_fpath);
      std::cout << "Current path is " << std::__fs::filesystem::current_path() << std::endl;


  if (!file.is_open()) {
    std::cerr << "Error opening file" << std::endl;
    return 1;
  }

  OCSCache *cache = new BasicOCSCache(10, 512, 1);

  std::string line;
  // Reading the header line
  getline(file, line);
  int header_lines = 2;

  // Reading each line of the file
  while (getline(file, line)) {
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

    //long timestamp = stol(columns[1]);
    long address = stol(columns[1]);
    //int count = stoi(columns[2]);
    std::string type = columns[3];

    bool hit = false;
    cache->handleMemoryAccess(address, &hit);
  }

  file.close();
  std::cout << *cache;
  return 0;
}
