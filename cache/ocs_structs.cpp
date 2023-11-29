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
  double hit_rate = stats.accesses > 0
                        ? static_cast<double>(stats.hits) / stats.accesses
                        : 0.0;
  double miss_rate = stats.accesses > 0
                         ? static_cast<double>(stats.misses) / stats.accesses
                         : 0.0;
  double promotion_rate = stats.candidates_created > 0
                              ? static_cast<double>(stats.candidates_promoted) /
                                    stats.candidates_created
                              : 0.0;
  os << "Cache Performance Summary:" << std::endl;
  os << "Total Accesses: " << stats.accesses << std::endl;

  os << "Hits: " << stats.hits << std::endl;
  os << "Misses: " << stats.misses << std::endl;
  os << "Hit Rate: " << hit_rate * 100 << "%" << std::endl;
  os << "Miss Rate: " << miss_rate * 100 << "%" << std::endl;

  os << "Cluster Candidates Promoted: " << stats.candidates_promoted << std::endl;
  os << "Cluster Candidates Created: " << stats.candidates_created << std::endl;
  os << "Candidate Promotion Rate: " << promotion_rate * 100 << "%" << std::endl;

  return os;
}
