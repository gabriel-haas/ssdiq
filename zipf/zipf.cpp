#include <iostream>
#include <random>
#include <vector>

#include "Env.hpp"
#include "RejectionInversionZipf.hpp"

int main() {
   long numberOfElements = getEnv("N", 1000);
   long samples = numberOfElements * 10;
   double exponent = getEnv("EXP", 0.5);
   std::random_device rd;
   std::mt19937_64 rng(rd());
   long buckets = getEnv("BUCKETS", 20);
   long elementsPerBucket = numberOfElements % buckets == 0 ? numberOfElements / buckets : (numberOfElements / buckets) + 1;

   RejectionInversionZipfSampler zipfSampler(numberOfElements, exponent);

   std::vector<long> accesses(numberOfElements, 0);
   for (long i = 0; i < samples; ++i) {
      long sample = zipfSampler.sample(rng);
      accesses[sample - 1]++;
   }

   // create histogram
   std::vector<long> histogram(buckets, 0);
   long nonZeros = 0;
   long cntAccesses = 0;
   for (long i = 0; i < numberOfElements; ++i) {
      volatile long j = i;
      volatile long e = elementsPerBucket;
      volatile long bucket = i / elementsPerBucket;
      histogram.at(bucket) += accesses[i];
      if (accesses[i] > 0) {
         nonZeros++;
      }
      cntAccesses += accesses[i];
   }
   if (cntAccesses != samples) {
      throw std::runtime_error("cntAccesses != samples");
   }

   //*
   // std::cout << std::setprecision(2) << std::fixed;
   std::cout << "type,i,cnt,percent" << std::endl;
   long cntAccessesHist = 0;
   for (long i = 0; i < buckets; ++i) {
      std::cout << "cpp," << i + 1 << "," << histogram[i] << "," << histogram[i] * 100.0 / samples << std::endl;
      cntAccessesHist += histogram[i];
   }
   if (cntAccessesHist != samples) {
      throw std::runtime_error("cntAccessesHist (" + std::to_string(cntAccessesHist) + ") != samples (" + std::to_string(samples) + ")");
   }
   //*/
   std::cout << "nonZeros: " << nonZeros << std::endl;

   return 0;
}
