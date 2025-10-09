#ifndef PARSE_FIU_TRACE_HPP
#define PARSE_FIU_TRACE_HPP

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <map>
#include <set>
#include <algorithm>
#include <unordered_set>
#include <filesystem>


struct FIUTraceEntry {
    uint64_t lba; // logical block address (in block unit)
    uint32_t blockCnt;
};


bool parseFIUTraceLine(const std::string& line, std::ofstream& out, uint32_t sectorSize, uint64_t pageSize, std::map<uint64_t, uint64_t>& histogram) {
    std::istringstream iss(line);
    std::string token;
    std::vector<std::string> tokens;

    // Split by spaces
    while (iss >> token) {
        tokens.push_back(token);
    }

    if (tokens.size() == 2 && std::stoul(tokens[1]) >= 8) {
        FIUTraceEntry entry;
        entry.lba = std::stoull(tokens[0]);
        entry.blockCnt = std::stoul(tokens[1]);

        uint64_t requestSize = entry.blockCnt * sectorSize;
        histogram[requestSize]++;

        uint64_t startOffset = entry.lba * sectorSize;
        uint64_t endOffset = startOffset + requestSize;
        for (uint64_t offset = startOffset; offset < endOffset; offset += pageSize) {
            uint64_t pageId = offset / pageSize;
            out << pageId << std::endl;
        }
        return true;
    }
    return false;
}

void parseAndWriteFIUTraceFile(const std::string& filename, const std::string& outputFile, uint32_t sectorSize, uint64_t pageSize, std::map<uint64_t, uint64_t>& histogram) {
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
        parseFIUTraceLine(line, out, sectorSize, pageSize, histogram);
    }

    infile.close();
    out.close();
    std::cout << "Trace file written: " << outputFile << std::endl;
}

void printFIURequestSizeHistogram(const std::string& traceinfoFilename, const std::string& parsedTraceFile, const std::map<uint64_t, uint64_t>& histogram, uint64_t pageSize, size_t* totalWriteCnt) {
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
    uint64_t sequentialWriteSize = 0;
    uint64_t lastEndLBA = 0;
    std::map<uint64_t, uint64_t> sequentialHistogram;

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

        // Check for sequential writes
        if (lastEndLBA != 0 && trace == lastEndLBA) {
            sequentialWriteSize += pageSize;
        } else {
            if (sequentialWriteSize > 0) {
                sequentialHistogram[sequentialWriteSize]++;
                sequentialWriteSize = 0;
            }
            sequentialWriteSize = pageSize;
        }
        lastEndLBA = trace + 1;
    }
    inFile.close();

    // Record the last sequential write if it exists
    if (sequentialWriteSize > 0) {
        sequentialHistogram[sequentialWriteSize]++;
    }

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

    traceinfo << "Sequential Write Size Histogram:" << std::endl;
    for (const auto& entry : sequentialHistogram) {
        traceinfo << entry.first << " bytes: " << entry.second << " requests" << std::endl;
    }

    traceinfo.close();

    std::cout << "FIU trace information has been written to " << traceinfoFilename << "." << std::endl;

    uniqueTraces.clear();
}

void validateAndLoadFIUTraces(const std::string& tracePath, uint32_t sectorSize, uint64_t logicalPages, uint64_t pageSize, const std::string& patternString, const std::string& parsedTraceFileName, size_t * totalWriteCnt) {

    std::map<uint64_t, uint64_t> histogram;
    parseAndWriteFIUTraceFile(tracePath, parsedTraceFileName, sectorSize, pageSize, histogram);
    //printFIURequestSizeHistogram(patternString + "_traceinfo.txt", parsedTraceFileName, histogram, pageSize, totalWriteCnt);
    histogram.clear();
}

#endif // PARSE_FIU_TRACE_HPP
