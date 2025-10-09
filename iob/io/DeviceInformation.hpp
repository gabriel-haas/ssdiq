#pragma once
#include <numeric>
#include <string>
#include <vector>
namespace mean {
struct DeviceInformation {
   struct Info {
      int id;
      std::string name;
      int fd = -1;
   };
   std::vector<Info> devices;
   std::string names() {
      return std::accumulate(devices.begin(), devices.end(), std::string(""), [](const std::string& acc, const Info& info) { return acc + info.name; });
   }
};
}; // namespace mean