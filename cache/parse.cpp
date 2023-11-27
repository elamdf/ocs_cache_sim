#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>

int main() {
    std::string filePath = "query_10_scale_1_rngseed_0_n1-highmem-8.csv"; // Replace with your CSV file path
    std::ifstream file(filePath);

    if (!file.is_open()) {
        std::cerr << "Error opening file" << std::endl;
        return 1;
    }

    std::string line;
    // Reading the header line
    getline(file, line);

    // Reading each line of the file
    while (getline(file, line)) {
        std::istringstream ss(line);
        std::string token;
        std::vector<std::string> columns;

        // Splitting line into columns
        while (getline(ss, token, ',')) {
            columns.push_back(token);
        }

        // Printing each column
        for (const auto& col : columns) {
            std::cout << col << " ";
        }
        std::cout << std::endl;
    }

    file.close();
    return 0;
}

