#ifndef PARSE_BLKTRACE_HPP
#define PARSE_BLKTRACE_HPP

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <map>
#include <unordered_set>
#include <algorithm>
#include <numeric>
#include <filesystem>


struct BlkTraceEntry {
    uint64_t blockId;
    uint32_t blockCount;
};

bool parseBlktraceLine(const std::string& line, std::ofstream& out, uint32_t sectorSize, uint64_t pageSize, std::map<uint64_t, uint64_t>& histogram) {
    std::istringstream iss(line);
    std::string token;
    std::vector<std::string> tokens;

    // Split by spaces
    while (iss >> token) {
        tokens.push_back(token);
    }

    // Check if the line contains at least 10 tokens and the operation is a write ('W')
    if (tokens.size() >= 10 && tokens[6] == "WS") {
        uint64_t blockId = std::stoull(tokens[7]);
        uint32_t blockCount = std::stoul(tokens[9]);

        uint64_t requestSize = blockCount * sectorSize;
        histogram[requestSize]++;

        uint64_t startOffset = blockId * sectorSize;
        uint64_t endOffset = startOffset + requestSize;
        for (uint64_t offset = startOffset; offset < endOffset; offset += pageSize) {
            uint64_t pageId = offset / pageSize;
            out << pageId << std::endl;
        }
        return true;
    }
    return false;
}

void parseAndWriteBlktraceFile(const std::string& filename, const std::string& outputFile, uint32_t sectorSize, uint64_t pageSize, std::map<uint64_t, uint64_t>& histogram) {
    std::ifstream infile(filename);
    if (!infile.is_open()) {
        std::cerr << "Error opening file: " << filename << std::endl;
        return;
    }

    std::ofstream out(outputFile);
    if (!out.is_open()) {
        throw std::runtime_error("Error opening output file.");
    }

    std::string line;
    while (std::getline(infile, line)) {
        parseBlktraceLine(line, out, sectorSize, pageSize, histogram);
    }

    infile.close();
    out.close();
    std::cout << "Trace file written: " << outputFile << std::endl;
}

void printBlktraceRequestSizeHistogram(const std::string& traceinfoFilename, const std::string& parsedTraceFile, const std::map<uint64_t, uint64_t>& histogram, uint64_t pageSize, size_t* totalWriteCnt) {
    std::ofstream traceinfo(traceinfoFilename, std::ios::app);
    if (!traceinfo.is_open()) {
        std::cerr << "Failed to open " << traceinfoFilename << " for writing." << std::endl;
        return;
    }

    std::unordered_set<uint64_t> uniqueTraces;
    uint64_t trace;
    uint64_t totalWriteRequestSize = 0;
    uint64_t maxIOOffset = 0;
    uint64_t minIOOffset = UINT64_MAX;

    // Calculate unique page IDs accessed and other metrics
    std::ifstream inFile(parsedTraceFile);
    if (!inFile.is_open()) {
        throw std::runtime_error("Error: Unable to open file for reading input traces.");
    }

    while (inFile >> trace) {
        uniqueTraces.insert(trace);
        totalWriteRequestSize += pageSize;
        maxIOOffset = std::max(maxIOOffset, trace * pageSize);
        minIOOffset = std::min(minIOOffset, trace * pageSize);
    }
    inFile.close();

    double maxIOOffsetGB = static_cast<double>(maxIOOffset) / (1024 * 1024 * 1024);
    double minIOOffsetGB = static_cast<double>(minIOOffset) / (1024 * 1024 * 1024);
    *totalWriteCnt = totalWriteRequestSize / pageSize;

    traceinfo << "Number of Unique Page IDs Accessed: " << uniqueTraces.size() << std::endl;
    traceinfo << "Total Write Request Size (page): " << totalWriteRequestSize / pageSize << " pages" << std::endl;
    traceinfo << "Maximum I/O Offset (page): " << maxIOOffset / pageSize << " (GB) " << maxIOOffsetGB << " GB" << std::endl;
    traceinfo << "Minimum I/O Offset (page): " << minIOOffset / pageSize << " (GB) " << minIOOffsetGB << " GB" << std::endl;
    traceinfo << "Request Size Histogram:" << std::endl;

    for (const auto& entry : histogram) {
        traceinfo << entry.first << " bytes: " << entry.second << " requests" << std::endl;
    }

    traceinfo.close();

    std::cout << "BlkTrace information has been written to " << traceinfoFilename << "." << std::endl;

    uniqueTraces.clear();
}

void validateAndLoadBlkTraces(const std::string& tracePath, uint32_t sectorSize, uint64_t logicalPages, uint64_t pageSize, const std::string& patternString , const std::string& parsedTraceFileName, size_t* totalWriteCnt) {

    std::map<uint64_t, uint64_t> histogram;
    parseAndWriteBlktraceFile(tracePath, parsedTraceFileName, sectorSize, pageSize, histogram);
    printBlktraceRequestSizeHistogram(patternString + "_traceinfo.txt", parsedTraceFileName, histogram, pageSize, totalWriteCnt);
    histogram.clear();
}

#endif // PARSE_BLKTRACE_HPP
