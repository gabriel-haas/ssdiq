#pragma once

#include "../IoInterface.hpp"

#include <array>
#include <cstdint>
#include <fcntl.h>
#include <filesystem>
#include <iostream>
#include <libnvme.h>
#include <nvme/types.h>
#include <regex>
#include <string>
#include <unistd.h>

// NOLINTBEGIN(modernize-*,performance-*)

#define C0_SMART_CLOUD_ATTR_LEN 0x200
#define C0_SMART_CLOUD_ATTR_OPCODE 0xC0
#define C0_GUID_LENGTH 16

namespace mean {
enum {
   SCAO_PMUW = 0,    /* Physical media units written */
   SCAO_PMUR = 16,   /* Physical media units read */
   SCAO_BUNBR = 32,  /* Bad user nand blocks raw */
   SCAO_BUNBN = 38,  /* Bad user nand blocks normalized */
   SCAO_BSNBR = 40,  /* Bad system nand blocks raw */
   SCAO_BSNBN = 46,  /* Bad system nand blocks normalized */
   SCAO_XRC = 48,    /* XOR recovery count */
   SCAO_UREC = 56,   /* Uncorrectable read error count */
   SCAO_SEEC = 64,   /* Soft ecc error count */
   SCAO_EEDC = 72,   /* End to end detected errors */
   SCAO_EECE = 76,   /* End to end corrected errors */
   SCAO_SDPU = 80,   /* System data percent used */
   SCAO_RFSC = 81,   /* Refresh counts */
   SCAO_MXUDEC = 88, /* Max User data erase counts */
   SCAO_MNUDEC = 92, /* Min User data erase counts */
   SCAO_NTTE = 96,   /* Number of Thermal throttling events */
   SCAO_CTS = 97,    /* Current throttling status */
   SCAO_EVF = 98,    /* Errata Version Field */
   SCAO_PVF = 99,    /* Point Version Field */
   SCAO_MIVF = 101,  /* Minor Version Field */
   SCAO_MAVF = 103,  /* Major Version Field */
   SCAO_PCEC = 104,  /* PCIe correctable error count */
   SCAO_ICS = 112,   /* Incomplete shutdowns */
   SCAO_PFB = 120,   /* Percent free blocks */
   SCAO_CPH = 128,   /* Capacitor health */
   SCAO_NEV = 130,   /* NVMe Errata Version */
   SCAO_UIO = 136,   /* Unaligned I/O */
   SCAO_SVN = 144,   /* Security Version Number */
   SCAO_NUSE = 152,  /* NUSE - Namespace utilization */
   SCAO_PSC = 160,   /* PLP start count */
   SCAO_EEST = 176,  /* Endurance estimate */
   SCAO_PLRC = 192,  /* PCIe Link Retraining Count */
   SCAO_PSCC = 200,  /* Power State Change Count */
   SCAO_LPV = 494,   /* Log page version */
   SCAO_LPG = 496,   /* Log page GUID */
};

class NvmeLog {
   bool ocpSupported = false;
   alignas(8) std::array<uint8_t, sizeof(__u8) * C0_SMART_CLOUD_ATTR_LEN> log_data;

   uint64_t toBigEndian(uint64_t value) {
      if (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__) {
         value = __builtin_bswap64(value);
      }
      return value;
   }
   uint32_t toBigEndian(uint32_t value) {
      if (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__) {
         value = __builtin_bswap32(value);
      }
      return value;
   }

 public:
   static std::string resolve_symlink(const std::string& path) {
      namespace fs = std::filesystem;
      fs::path p = fs::absolute(path);
      while (fs::is_symlink(p)) {
         fs::path link_target = fs::read_symlink(p);
         if (link_target.is_relative()) {
            p = fs::canonical(p.parent_path() / link_target);
         } else {
            p = fs::canonical(link_target);
         }
      }
      return fs::canonical(p).string();
   }
   static std::string extract_nvme_controller(const std::string& resolved_path) {
      std::regex nvme_regex(R"(\/dev\/(nvme\d+)n\d+)");
      std::smatch match;
      if (std::regex_match(resolved_path, match, nvme_regex)) {
         return "/dev/" + match[1].str();
      }
      return resolved_path; // fallback: no change
   }
   void loadOCPSmartLog() {
      std::string name = IoInterface::instance().getDeviceInfo().devices[0].name;
      // name is like /blk/d1 or /dev/nvmeXn1
      // get the controler name, like /dev/nvmeX
      std::string resolved = resolve_symlink(name);
      std::string controller = extract_nvme_controller(resolved);
      std::cout << "path: " << name << " resolved: " << resolved << " controller: " << controller << std::endl;
      int fd = open(controller.c_str(), O_RDONLY | O_CLOEXEC);
      if (fd >= 0) {
         int ret = nvme_get_log_simple(fd, (nvme_cmd_get_log_lid)C0_SMART_CLOUD_ATTR_OPCODE, C0_SMART_CLOUD_ATTR_LEN, &log_data); // NOLINT
         ocpSupported = (ret == 0);
         close(fd);
      } else {
         ocpSupported = false;
      }
   }
   uint64_t physicalMediaUnitsWrittenBytes() {
      if (!ocpSupported) {
         return 0;
      }
      // ensure(!ocpSupported || toBigEndian(*(uint64_t *)&log_data[SCAO_PMUW + 8] & 0xFFFFFFFFFFFFFFFF) == 0);
      return (toBigEndian(*(uint64_t*)&log_data[SCAO_PMUW] & 0xFFFFFFFFFFFFFFFF));
   }
   uint64_t physicalMediaUnitsReadBytes() {
      if (!ocpSupported) {
         return 0;
      }
      // ensure(!ocpSupported || (uint64_t)toBigEndian(*(uint64_t *)&log_data[SCAO_PMUR + 8] & 0xFFFFFFFFFFFFFFFF) == 0);
      return toBigEndian(*(uint64_t*)&log_data[SCAO_PMUR] & 0xFFFFFFFFFFFFFFFF);
   }
   uint64_t softECCError() {
      return toBigEndian(*(uint64_t*)&log_data[SCAO_SEEC]);
   }
   uint64_t unalignedIO() {
      return toBigEndian(*(uint64_t*)&log_data[SCAO_UIO]);
   }
   uint64_t maxUserDataEraseCount() {
      return toBigEndian(*(uint32_t*)&log_data[SCAO_MXUDEC]);
   }
   uint64_t minUserDataEraseCount() {
      return toBigEndian(*(uint32_t*)&log_data[SCAO_MNUDEC]);
   }
   uint8_t percentFreeBlocks() {
      return (__u8)log_data[SCAO_PFB];
   }
   uint8_t currentThrottlingStatus() {
      return (__u8)log_data[SCAO_CTS];
   }
};
// NOLINTEND(modernize-*,performance-*)
}; // namespace mean
