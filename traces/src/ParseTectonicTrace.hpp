#ifndef PARSE_TECTONIC_TRACE_HPP
#define PARSE_TECTONIC_TRACE_HPP

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <map>
#include <algorithm>

struct TectonicTraceEntry {
    uint64_t blockId;
    uint64_t ioOffset;
    uint32_t ioSize;
    double opTime;
    uint32_t opName;
    uint64_t userNamespace;
    uint64_t userName;
    uint32_t rsShardId;
    uint32_t opCount;
};

const uint64_t BLOCK_SIZE = 4 * 1024; // 72KB
const std::vector<uint32_t> PUT_OPS = {3, 4, 6};

bool parseTectonicTraceLine(const std::string& line, TectonicTraceEntry& entry) {
    std::istringstream iss(line);
    if (!(iss >> entry.blockId >> entry.ioOffset >> entry.ioSize >> entry.opTime >> entry.opName >>
            entry.userNamespace >> entry.userName >> entry.rsShardId >> entry.opCount)) {
        return false;
    }
    return true;
}

std::vector<TectonicTraceEntry> parseTectonicTraceFile(const std::string& filename) {
    std::ifstream infile(filename);
    if (!infile.is_open()) {
        std::cerr << "Error opening file: " << filename << std::endl;
        return {};
    }

    std::string line;
    std::vector<TectonicTraceEntry> writeRequests;

    while (std::getline(infile, line)) {
        TectonicTraceEntry entry;
        if (parseTectonicTraceLine(line, entry)) {
            if (std::find(PUT_OPS.begin(), PUT_OPS.end(), entry.opName) != PUT_OPS.end()) {
                writeRequests.push_back(entry);
            }
        }
    }

    infile.close();
    return writeRequests;
}

void printTectonicTraceEntries(const std::vector<TectonicTraceEntry>& traceEntries) {
    std::cout << "Trace Entries:" << std::endl;
    for (const auto& entry : traceEntries) {
        std::cout << "Block ID: " << entry.blockId 
                  << ", IO Offset: " << entry.ioOffset 
                  << ", IO Size: " << entry.ioSize 
                  << ", Op Time: " << entry.opTime 
                  << ", Op Name: " << entry.opName 
                  << ", User Namespace: " << entry.userNamespace 
                  << ", User Name: " << entry.userName 
                  << ", RS Shard ID: " << entry.rsShardId 
                  << ", Op Count: " << entry.opCount << std::endl;
    }
}

uint64_t calculateMinimumDatasetCapacity(const std::vector<TectonicTraceEntry>& traceEntries) {
    uint64_t minBlock = UINT64_MAX;
    uint64_t maxBlock = 0;

    for (const auto& entry : traceEntries) {
        uint64_t startOffset = entry.blockId * BLOCK_SIZE + entry.ioOffset;
        uint64_t endOffset = startOffset + entry.ioSize;
        minBlock = std::min(minBlock, startOffset);
        maxBlock = std::max(maxBlock, endOffset);
    }

    return (maxBlock - minBlock);
}

uint64_t calculateMinimumSSDCapacity(const std::vector<TectonicTraceEntry>& traceEntries) {
    uint64_t maxBlock = 0;

    for (const auto& entry : traceEntries) {
        uint64_t startOffset = entry.blockId * BLOCK_SIZE + entry.ioOffset;
        uint64_t endOffset = startOffset + entry.ioSize;
        maxBlock = std::max(maxBlock, endOffset);
    }

    return maxBlock;
}

uint64_t MaximumRequestSize(const std::vector<TectonicTraceEntry>& traceEntries) {
    uint64_t maxSize = 0;
    for (const auto& entry : traceEntries) {
        maxSize = std::max(maxSize, static_cast<uint64_t>(entry.ioSize));
    }
    return maxSize;
}

void printRequestSizeHistogram(const std::vector<TectonicTraceEntry>& traceEntries) {
    std::map<uint32_t, uint32_t> histogram;
    for (const auto& entry : traceEntries) {
        histogram[entry.ioSize]++;
    }
    
    std::cout << "Request Size Histogram:" << std::endl;
    for (const auto& [size, count] : histogram) {
        std::cout << "Size: " << size << ", Count: " << count << std::endl;
    }
}

#endif // PARSE_TECTONIC_TRACE_HPP
