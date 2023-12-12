#ifndef COMMAND_LINE_OPTIONS_H
#define COMMAND_LINE_OPTIONS_H

#include <string>
#include <boost/program_options.hpp>

class CLIOpts {
public:
    // Constructor
    CLIOpts();

    // Parse the command line arguments
    void parse(int argc, char* argv[]);

    // Getters for the command line options
    std::string getInputFile() const;
    int getNumLines() const;
    int getSampleNumLines() const;
    std::string getOutputFile() const;
    bool enableVerboseOutput() const;

private:
    std::string inputFile;
    boost::program_options::options_description desc;

    int num_lines = -1;
    int sample_num_lines = -1;
    bool verbose = true;
    std::string outputFile = "";

    boost::program_options::variables_map vm;
};

#endif // COMMAND_LINE_OPTIONS_H

