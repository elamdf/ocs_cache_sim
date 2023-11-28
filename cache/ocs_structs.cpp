#include "ocs_structs.h"

std::ostream& operator<<(std::ostream& os, const addr_subspace& subspace) {
    os << "Address Subspace { Start: 0x" << std::hex << subspace.addr_start 
       << ", End: 0x" << std::hex << subspace.addr_end << " }";
    return os;
}

std::ostream& operator<<(std::ostream& os, const candidate_cluster& cluster) {
    os << "Candidate Cluster {\n"
       << "  Range: " << cluster.range << ",\n"
       << "  On-Cluster Accesses: " << std::dec << cluster.on_cluster_accesses << ",\n"
       << "  Off-Cluster Accesses: " << cluster.off_cluster_accesses << ",\n"
       << "  Valid: " << (cluster.valid ? "true" : "false") << "\n"
       << "}";
    return os;
}

std::ostream& operator<<(std::ostream& os, const pool_entry& entry) {
    os << "Pool Entry {\n"
       << "  Range: " << entry.range << ",\n"
       << "  Valid: " << (entry.valid ? "true" : "false") << ",\n"
       << "  In Cache: " << (entry.in_cache ? "true" : "false") << "\n"
       << "}";
    return os;
}

