#ifndef PARSE_TRACES_HPP
#define PARSE_TRACES_HPP

#include "ParseBlktrace.hpp"
#include "ParseAlibabaTrace.hpp"
#include "ParseFIUTrace.hpp"

#include <vector>
#include <string>
#include <numeric>
#include <unordered_set>
#include <iostream>
#include <fstream>
#include <map>
#include <filesystem>

namespace iob {

namespace fs = std::filesystem;

std::string parsedTraceFile;
size_t totalWriteCnt = 0;
uint64_t maxPid = 0;

std::string getTraceFilePath(const std::string& patternString) {
    std::string traceFile;
        size_t pos = patternString.find("_");
        if (pos != std::string::npos) {
            std::string traceName = patternString.substr(pos + 1);
            return "../traces/tracefiles/" + traceName + "/writetrace";
        } else {
            throw std::runtime_error("Error: Trace file not specified and could not determine from pattern string.");
        }
    return traceFile;
}

std::string getTraceParsedTraceFilePath(const std::string& patternString) {
    return patternString + "_input_traces.txt";
}

// Function to fetch pages from the parsed trace file in chunks
// Function to fetch pages from the parsed trace file in chunks
bool fetchPagesFromParsedTrace(std::vector<uint64_t>& inputTraces, size_t chunkSize, size_t& traceIndex) {
    static std::ifstream inFile(parsedTraceFile);
    if (!inFile.is_open()) {
        std::cerr << "Error: Unable to open file " << parsedTraceFile << " for reading input traces." << std::endl;
        return false;
    }

    inputTraces.clear();
    inputTraces.reserve(chunkSize);

    uint64_t trace;
    size_t count = 0;

    while (count < chunkSize) {
        if (!(inFile >> trace)) {
            // Reached end of file, rewind and try again
            std::cout << "\n[Trace] Reached end of file, rewinding to beginning." << std::endl;
            inFile.clear();                 // Clear EOF flag
            traceIndex = 0;
            inFile.seekg(0, std::ios::beg); // Go back to beginning
            continue;                       // Retry reading
        }

        inputTraces.push_back(trace);
        count++;
    }

    // if (count > 0) {
    //     std::cout << "[Trace] Loaded chunk of " << count << " traceIndex: " << traceIndex 
    //               << ", first trace: " << inputTraces.front()
    //               << ", last trace: " << inputTraces.back() << std::endl;
    // }

    return !inputTraces.empty();
}


// Function to get a page from the parsed trace file
// Function to get a page from the parsed trace file
uint64_t getPageFromParsedTrace(std::vector<uint64_t>& inputTraces, size_t& traceIndex, size_t chunkSize) {
    if (traceIndex % chunkSize == 0) {
        if (!fetchPagesFromParsedTrace(inputTraces, chunkSize, traceIndex)) {
            throw std::runtime_error("Error: Unable to fetch more pages from the parsed trace file.");
        }
    }

    traceIndex++;
    uint64_t page = inputTraces[traceIndex % chunkSize];

    // std::cout << "[Trace] traceIndex = " << traceIndex 
    //           << ", chunkSize = " << chunkSize 
    //           << ", page = " << page << std::endl;

    return page;
}

void generateAccessFrequencyHistogram(const std::string& parsedTraceFile, const std::string& outputFilename, const std::string& patternString) {
    std::string csvOutputFilename = patternString + "_access_frequency.csv";
    std::ifstream inFile(parsedTraceFile);
    if (!inFile.is_open()) {
        throw std::runtime_error("Error: Unable to open file for reading input traces.");
    }

    std::map<uint64_t, uint64_t> frequencyMap;
    uint64_t pageId;
    while (inFile >> pageId) {
        frequencyMap[pageId]++;
    }
    inFile.close();

    // Convert the frequency map to a vector of pairs
    std::vector<std::pair<uint64_t, uint64_t>> frequencyVector(frequencyMap.begin(), frequencyMap.end());

    // Sort the vector by frequency in descending order
    std::sort(frequencyVector.begin(), frequencyVector.end(), [](const auto& a, const auto& b) {
        return b.second > a.second;
    });

    // Create 10 buckets and calculate the percentage of access for each bucket
    size_t bucketSize = (frequencyVector.size() + 9) / 10; // Round up to ensure we have 10 buckets
    std::vector<double> bucketPercentages(10, 0.0);

    for (size_t i = 0; i < 10; ++i) {
        size_t startIdx = i * bucketSize;
        size_t endIdx = std::min(startIdx + bucketSize, frequencyVector.size());

        double bucketSum = std::accumulate(frequencyVector.begin() + startIdx, frequencyVector.begin() + endIdx, 0ULL, [](uint64_t sum, const auto& entry) {
            return sum + entry.second;
        });

        bucketPercentages[i] = (bucketSum / totalWriteCnt) * 100.0; // Use totalWriteCnt to calculate percentage
    }

    // Reverse the order of the buckets to display the largest first
    std::reverse(bucketPercentages.begin(), bucketPercentages.end());

    // Write frequency data to a CSV file for R processing
    std::ofstream csvFile(csvOutputFilename);
    if (!csvFile.is_open()) {
        std::cerr << "Error: Unable to open file for writing frequency data." << std::endl;
        return;
    }

    csvFile << "Bucket,Percentage\n";
    for (size_t i = 0; i < 10; ++i) {
        csvFile << i + 1 << "," << bucketPercentages[i] << "\n";
    }
    csvFile.close();

    // Generate R script to create the histogram
  // Generate R script to create the histogram
    std::string rScript =
        "library(ggplot2)\n"
        "data <- read.csv('" + csvOutputFilename + "')\n"
        "ggplot(data, aes(x=Bucket, y=Percentage)) + "
        "geom_bar(stat='identity', fill='#0073C2FF') + " // Set the fill color similar to ggplot2
        "theme_minimal() + "
        "theme(panel.grid.major = element_line(color='gray', linewidth=0.3), panel.grid.minor = element_line(color='gray', linewidth=0.1), panel.border = element_rect(colour = 'black', fill=NA, linewidth=0.5)) + "
        "ggtitle('" + patternString + "')+ "
        "xlab('LBA range group (1: 10%)') + "
        "ylab('Access Frequency (%)') + "
        "scale_x_continuous(breaks=1:20, labels=1:20) + "
        "scale_y_continuous(limits=c(0, 100), breaks=seq(0, 100, by=10))\n"
        "ggsave('" + patternString + ".png', width=4, height=2)\n"; // Specify width and height in inches
     
    // Write the R script to a file
    std::ofstream rFile("plot_histogram.R");
    if (!rFile.is_open()) {
        std::cerr << "Error: Unable to open file for writing R script." << std::endl;
        return;
    }

    rFile << rScript;
    rFile.close();

    // Execute the R script
    std::system("Rscript plot_histogram.R");
}


void validateAndLoadTraceFiles(const std::string& tracePath, const std::string& patternString, uint32_t sectorSize, uint64_t logicalPages, uint64_t pageSize, std::vector<uint64_t>& inputTraces) {
    parsedTraceFile = getTraceParsedTraceFilePath(patternString);

    // Check if the parsed trace file exists and delete it
    if (fs::exists(parsedTraceFile)) {
        return;
    }

    maxPid = logicalPages;

    if (patternString.find("RocksDBYCSB") != std::string::npos || patternString.find("LeanStoreTPCC") != std::string::npos || 
        patternString.find("MySQLTPCC") != std::string::npos || patternString.find("RocksDBDBench") != std::string::npos) {
        validateAndLoadBlkTraces(tracePath, sectorSize, logicalPages, pageSize, patternString, parsedTraceFile, &totalWriteCnt);
    } else if (patternString.find("Alibaba") != std::string::npos || patternString.find("MSRCambridge") != std::string::npos) {
        validateAndLoadAlibabaTraces(tracePath, logicalPages, pageSize, patternString, parsedTraceFile, &totalWriteCnt);
    } else if (patternString.find("FIU") != std::string::npos) {
        validateAndLoadFIUTraces(tracePath, sectorSize, logicalPages, pageSize, patternString, parsedTraceFile, &totalWriteCnt);
    } else {
        throw std::runtime_error("Unsupported trace type in pattern string.");
    }
    parsedTraceFile = getTraceParsedTraceFilePath(patternString);
    std::cout << "Parsed trace file: " << parsedTraceFile << std::endl;
    std::string csvFile = patternString + "access_frequency.csv";
    generateAccessFrequencyHistogram(parsedTraceFile, csvFile, patternString);
}

} // namespace iob

#endif // PARSE_TRACES_HPP
