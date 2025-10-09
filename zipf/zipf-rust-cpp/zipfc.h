#ifndef BTREE24_ZIPFC_H
#define BTREE24_ZIPFC_H

#include <cstdint>

extern "C" {

struct ZipfcRng;
struct ZipfPermutation;

struct Key {
    uint8_t *data;
    uint64_t len;

    std::span<uint8_t> span() {
        return {data, len};
    }
};

Key *zipfc_load_keys(ZipfcRng *rng, const char *name, uint32_t count, double density, uint32_t partition_count);

ZipfcRng *create_zipfc_rng(uint64_t u64, uint64_t thread, const char *purpose);

uint32_t *generate_zipf_indices(
        ZipfcRng *rng,
        uint32_t key_count,
        double zipf_parameter,
        uint32_t sample_count
);

void fill_u64_single_thread(ZipfcRng *rng, uint64_t *data, uint64_t count);

ZipfPermutation *create_zipf_permutation(ZipfcRng *rng, uint32_t count, double zipf);
void fill_zipf_single_thread(ZipfcRng *rng, ZipfPermutation *dist, uint32_t *dst, uint64_t count);

}

#endif //BTREE24_ZIPFC_H
