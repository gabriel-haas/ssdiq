#pragma once

#include <cmath>
#include <functional>
#include <iostream>
#include <iterator>
#include <mutex>
#include <numeric>
#include <stdexcept>
#include <vector>

template <typename storageType, typename valueType>
class Hist {
 public:
   const int size;
   valueType from = 0;
   valueType to = 1;
   std::mutex lock;

   valueType min;
   valueType max;

   int64_t cnt;
   double total;

   std::vector<storageType> histData;
   Hist(storageType size = 1, valueType from = 0, valueType to = 1) : size(size), from(from), to(to), min(to), max(from) {
      std::unique_lock<std::mutex> ul(lock);
      histData.resize(size);
   }

   Hist(const Hist&) = delete;
   Hist& operator=(Hist&) = delete;

   int increaseSlot(valueType value) {
      value = std::max(value, from);
      value = std::min(value, to - 1);
      const valueType normalizedValue = (value - from) / (double)(to - from) * size;
      histData.at(normalizedValue)++;
      min = value < min ? value : min;
      max = value > max ? value : max;
      total += value;
      cnt++;
      return normalizedValue;
   }

   void print() {
      // std::cout << "[";
      // std::cout << "(size:" << size << ",from:" << from << ",to:" << to << "),";
      std::copy(histData.begin(),
                histData.end(),
                std::ostream_iterator<storageType>(std::cout, ","));
      // std::cout << "]";
   }

   valueType getPercentile(float iThPercentile) {
      std::unique_lock<std::mutex> ul(lock);
      long sum = std::accumulate(histData.begin(), histData.end(), 0, std::plus<>());
      long sumUntilPercentile = 0;
      const long percentile = sum * (iThPercentile / 100.0);
      valueType i = 0;
      for (; sumUntilPercentile < percentile && i < histData.size(); i++) {
         sumUntilPercentile += histData[i];
      }
      return from + (i / (float)size * (to - from));
   }

   // Function to write percentiles header as a string
   void writePercentilesHeader(const std::string& prefix, std::string& result) {
      result += prefix + "min,";
      result += prefix + "10p,";
      result += prefix + "20p,";
      result += prefix + "25p,";
      result += prefix + "30p,";
      result += prefix + "40p,";
      result += prefix + "50p,";
      result += prefix + "60p,";
      result += prefix + "70p,";
      result += prefix + "75p,";
      result += prefix + "80p,";
      result += prefix + "85p,";
      result += prefix + "90p,";
      result += prefix + "92p5,";
      result += prefix + "95p,";
      result += prefix + "97p5,";
      result += prefix + "99p,";
      result += prefix + "99p5,";
      result += prefix + "99p9,";
      result += prefix + "99p99,";
      result += prefix + "99p999,";
      result += prefix + "max,";
      result += prefix + "avg,";
      result += prefix + "tot,";
      result += prefix + "cnt";
   }

   // Function to write percentiles as a string
   void writePercentiles(std::string& result) {
      result += std::to_string(min) + ",";
      result += std::to_string(getPercentile(10)) + ",";
      result += std::to_string(getPercentile(20)) + ",";
      result += std::to_string(getPercentile(25)) + ",";
      result += std::to_string(getPercentile(30)) + ",";
      result += std::to_string(getPercentile(40)) + ",";
      result += std::to_string(getPercentile(50)) + ",";
      result += std::to_string(getPercentile(60)) + ",";
      result += std::to_string(getPercentile(70)) + ",";
      result += std::to_string(getPercentile(75)) + ",";
      result += std::to_string(getPercentile(80)) + ",";
      result += std::to_string(getPercentile(85)) + ",";
      result += std::to_string(getPercentile(90)) + ",";
      result += std::to_string(getPercentile(92.5)) + ",";
      result += std::to_string(getPercentile(95)) + ",";
      result += std::to_string(getPercentile(97.5)) + ",";
      result += std::to_string(getPercentile(99)) + ",";
      result += std::to_string(getPercentile(99.5)) + ",";
      result += std::to_string(getPercentile(99.9)) + ",";
      result += std::to_string(getPercentile(99.99)) + ",";
      result += std::to_string(getPercentile(99.999)) + ",";
      result += std::to_string(max) + ",";
      if (cnt != 0) {
         result += std::to_string(total / cnt) + ",";
      } else {
         result += std::to_string(0) + ",";
      }
      result += std::to_string(total) + ",";
      result += std::to_string(cnt);
   }

   Hist& operator+=(const Hist<storageType, valueType>& rhs) {
      if (size != rhs.size || from != rhs.from || to != rhs.to) {
         throw std::logic_error("Cannot add two different hists.");
      }
      for (int i = 0; i < size; i++) {
         this->histData[i] += rhs.histData[i];
      }
      total += rhs.total;
      cnt += rhs.cnt;
      return *this;
   }

   Hist& operator+(const Hist<storageType, valueType>& rhs) {
      auto result = *this;
      return result += rhs;
   }

   void resetData() {
      std::fill(histData.begin(), histData.end(), 0);
      min = std::numeric_limits<valueType>::max();
      max = std::numeric_limits<valueType>::min();
      cnt = 0;
      total = valueType();
   }

 private:
};
