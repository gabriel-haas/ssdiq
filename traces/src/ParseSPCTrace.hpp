#ifndef SPC_TRACE_PARSE_HPP
#define SPC_TRACE_PARSE_HPP

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <map>
#include <algorithm>
#include <cstdint>

struct SPCTraceEntry {
    int asu;
    uint64_t blockId;
    uint32_t byteCount;
    char operation;
    double timestamp;
    std::vector<std::string> optionalFields;
};

bool parseSPCTraceLine(const std::string& line, SPCTraceEntry& entry) {
    std::istringstream iss(line);
    std::string token;
    std::vector<std::string> tokens;

    // Split by comma
    while (std::getline(iss, token, ',')) {
        tokens.push_back(token);
    }

    if (tokens.size() < 5) {
        return false;  // Minimum 5 fields are required
    }

    try {
        entry.asu = std::stoi(tokens[0]);
        entry.blockId = std::stoull(tokens[1]);
        entry.byteCount = std::stoul(tokens[2]);
        entry.operation = tokens[3][0];
        entry.timestamp = std::stod(tokens[4]);

        // Store any optional fields
        entry.optionalFields.assign(tokens.begin() + 5, tokens.end());
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error parsing line: " << e.what() << std::endl;
        return false;
    }
}

std::vector<SPCTraceEntry> parseSPCTraceFile(const std::string& filename) {
    std::ifstream infile(filename);
    if (!infile.is_open()) {
        std::cerr << "Error opening file: " << filename << std::endl;
        return {};
    }

    std::string line;
    std::vector<SPCTraceEntry> traceEntries;

    while (std::getline(infile, line)) {
        SPCTraceEntry entry;
        if (parseSPCTraceLine(line, entry)) {
            traceEntries.push_back(entry);
        }
    }

    infile.close();
    return traceEntries;
}

void printSPCTraceEntries(const std::vector<SPCTraceEntry>& traceEntries) {
    std::cout << "SPC Trace Entries:" << std::endl;
    for (const auto& entry : traceEntries) {
        std::cout << "ASU: " << entry.asu 
                  << ", Block ID: " << entry.blockId 
                  << ", Byte Count: " << entry.byteCount 
                  << ", Operation: " << entry.operation 
                  << ", Timestamp: " << entry.timestamp;
        
        if (!entry.optionalFields.empty()) {
            std::cout << ", Optional Fields: ";
            for (const auto& field : entry.optionalFields) {
                std::cout << field << " ";
            }
        }

        std::cout << std::endl;
    }
}

uint64_t calculateMinimumDatasetCapacity(const std::vector<SPCTraceEntry>& traceEntries) {
    uint64_t minBlock = UINT64_MAX;
    uint64_t maxBlock = 0;

    for (const auto& entry : traceEntries) {
        minBlock = std::min(minBlock, entry.blockId);
        maxBlock = std::max(maxBlock, entry.blockId + entry.byteCount);
    }

    return maxBlock - minBlock;
}

uint64_t calculateMinimumSSDCapacity(const std::vector<SPCTraceEntry>& traceEntries) {
    uint64_t maxBlock = 0;

    for (const auto& entry : traceEntries) {
        maxBlock = std::max(maxBlock, entry.blockId + entry.byteCount);
    }

    return maxBlock;
}

uint64_t MaximumRequestSize(const std::vector<SPCTraceEntry>& traceEntries) {
    uint64_t maxSize = 0;
    for (const auto& entry : traceEntries) {
        maxSize = std::max(maxSize, static_cast<uint64_t>(entry.byteCount));
    }
    return maxSize;
}

void printRequestSizeHistogram(const std::vector<SPCTraceEntry>& traceEntries) {
    std::map<uint64_t, int> histogram;
    for (const auto& entry : traceEntries) {
        histogram[entry.byteCount]++;
    }

    std::cout << "Request Size Histogram:" << std::endl;
    for (const auto& [size, count] : histogram) {
        std::cout << size << " bytes: " << count << " requests" << std::endl;
    }
}

#endif // SPC_TRACE_PARSE_HPP
