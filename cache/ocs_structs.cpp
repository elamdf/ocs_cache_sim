#include "ocs_structs.h"

std::ostream &operator<<(std::ostream &os, const addr_subspace &subspace) {
  os << "Address Subspace { Start: 0x" << std::hex << subspace.addr_start
     << ", End: 0x" << std::hex << subspace.addr_end << " }";
  return os;
}

std::ostream &operator<<(std::ostream &os, const candidate_cluster &cluster) {
  os << "Candidate Cluster {\n"
     << "  Id: " << cluster.id << ",\n"
     << "  Range: " << cluster.range << ",\n"
     << "  On-Cluster Accesses: " << std::dec << cluster.on_cluster_accesses
     << ",\n"
     << "  Off-Cluster Accesses: " << cluster.off_cluster_accesses << ",\n"
     << "  Valid: " << (cluster.valid ? "true" : "false") << "\n"
     << "}";
  return os;
}

std::ostream &operator<<(std::ostream &os, const pool_entry &entry) {
  os << "Pool Entry {\n"
     << "  Id: " << entry.id << ",\n"
     << "  Range: " << entry.range << ",\n"
     << "  Valid: " << (entry.valid ? "true" : "false") << ",\n"
     << "  In Cache: " << (entry.in_cache ? "true" : "false") << "\n"
     << "}";
  return os;
}

std::ostream &operator<<(std::ostream &os, const perf_stats &stats) {

  long backing_store_accesses =
      (stats.backing_store_pool_hits + stats.backing_store_misses);
  long ocs_accesses = (stats.ocs_pool_hits + stats.ocs_reconfigurations);

  double ocs_hit_rate =
      ocs_accesses > 0 ? static_cast<double>(stats.ocs_pool_hits) / ocs_accesses
                       : 0.0;

  double ocs_utilization = static_cast<double>(ocs_accesses) / stats.accesses;
  double backing_store_utilization =
      static_cast<double>(backing_store_accesses) / stats.accesses;

  double backing_hit_rate =
      backing_store_accesses > 0
          ? static_cast<double>(stats.backing_store_pool_hits) /
                backing_store_accesses
          : 0.0;

  double promotion_rate = stats.candidates_created > 0
                              ? static_cast<double>(stats.candidates_promoted) /
                                    stats.candidates_created
                              : 0.0;
  long total_off_node_mem_usage =
      stats.ocs_pool_mem_usage + stats.backing_store_mem_usage;

  if (!stats.summary) { // we want more than the summary
    os << "\n------------------------------"
          "Performance------------------------------\n";
    os << "Cache Performance Summary:" << std::endl;
    os << "OCS Pool Memory Usage (Assuming no Defragmentation): "
       << stats.ocs_pool_mem_usage << std::endl;
    os << "OCS Pool Memory Usage (Assuming no Defragmentation): "
       << stats.backing_store_mem_usage << std::endl;
    os << "Total off-node Memory Usage (Assuming no Defragmentation): "
       << total_off_node_mem_usage << std::endl;
    os << "Memory Latency: "
       << "TODO" << std::endl;
    os << std::endl;

    os << "Total Accesses: " << stats.accesses << std::endl;
    os << "DRAM Accesses: " << stats.dram_hits << std::endl;

    os << "\n------------------------------OCS "
          "Performance------------------------------\n";
    os << "OCS Utilization: " << ocs_utilization * 100 << "%" << std::endl;
    os << "OCS Pool Hits: " << stats.ocs_pool_hits << std::endl;
    os << "OCS Hit Rate: " << ocs_hit_rate * 100 << "%" << std::endl;
    os << "OCS Reconfigurations: " << stats.ocs_reconfigurations << std::endl;

    os << "\n------------------------------Backing Store "
          "Performance------------------------------\n";
    os << "Backing Store Utilization: " << backing_store_utilization * 100
       << "%" << std::endl;
    os << "Backing Store Pool Hits: " << stats.backing_store_pool_hits
       << std::endl;
    os << "Backing Store Hit Rate: " << backing_hit_rate * 100 << "%"
       << std::endl;
    os << "Backing Store Misses: " << stats.backing_store_misses << std::endl;

    os << "\n------------------------------Clustering Policy "
          "Performance------------------------------\n";
    os << "Cluster Candidates Promoted: " << stats.candidates_promoted
       << std::endl;
    os << "Cluster Candidates Created: " << stats.candidates_created
       << std::endl;
    os << "Candidate Promotion Rate: " << promotion_rate * 100 << "%"
       << std::endl;
    // TODO figure out + report what % of memory is now off DRAM
    // find a way to make ^ honest, it's dishonest on its own since we don't
    // know _how long_ a given memory address has been in the pool vs in the
    // DRAM, we'd just be reporting its  location at the end likely need to do
    // something with timestamps
  }

  os << "\n------------------------------"
        "Performance Summary---------------------------\n";
  os << "Overall Memory Latency: "
     << "TODO" << std::endl;
  os << "Total off-node Memory Usage (Assuming no Defragmentation): "
     << total_off_node_mem_usage << "B" << std::endl;

  os << "OCS Utilization: " << ocs_utilization * 100 << "%" << std::endl;
  os << "OCS Hit Rate: " << ocs_hit_rate * 100 << "%" << std::endl;
  os << "Backing Store Utilization: " << backing_store_utilization * 100 << "%"
     << std::endl;
  os << "Backing Store Hit Rate: " << backing_hit_rate * 100 << "%"
     << std::endl;


  return os;
}
