#include "equihash.hpp"
#include "util.hpp"

#include <algorithm>
#include <cstring>

void eh_init_state(Blake2bState* S, const uint8_t header_prefix[180], const uint8_t nonce[32]) {
    blake2b_init_equihash96_5(S);
    blake2b_update(S, header_prefix, 180);
    blake2b_update(S, nonce, 32);
}

void eh_generate_hash(const Blake2bState& base, uint32_t g, uint8_t out[60]) {
    Blake2bState S = base;
    uint8_t gbytes[4];
    store32_le(gbytes, g);
    blake2b_update(&S, gbytes, 4);
    blake2b_final(&S, out, 60);
}

void ExpandArray(const unsigned char* in, size_t in_len,
                 unsigned char* out, size_t out_len,
                 size_t bit_len, size_t byte_pad) {
    size_t out_width = (bit_len + 7) / 8 + byte_pad;
    uint32_t bit_len_mask = ((uint32_t)1 << bit_len) - 1;
    size_t acc_bits = 0;
    uint32_t acc_value = 0;
    size_t j = 0;
    for (size_t i = 0; i < in_len; i++) {
        acc_value = (acc_value << 8) | in[i];
        acc_bits += 8;
        if (acc_bits >= bit_len) {
            acc_bits -= bit_len;
            for (size_t x = 0; x < byte_pad; x++) out[j + x] = 0;
            for (size_t x = byte_pad; x < out_width; x++) {
                out[j + x] = (uint8_t)(
                    (acc_value >> (acc_bits + (8 * (out_width - x - 1)))) &
                    ((bit_len_mask >> (8 * (out_width - x - 1))) & 0xFF));
            }
            j += out_width;
        }
    }
    (void)out_len;
}

void CompressArray(const unsigned char* in, size_t in_len,
                   unsigned char* out, size_t out_len,
                   size_t bit_len, size_t byte_pad) {
    size_t in_width = (bit_len + 7) / 8 + byte_pad;
    uint32_t bit_len_mask = ((uint32_t)1 << bit_len) - 1;
    size_t acc_bits = 0;
    uint32_t acc_value = 0;
    size_t j = 0;
    for (size_t i = 0; i < out_len; i++) {
        if (acc_bits < 8) {
            acc_value = acc_value << bit_len;
            for (size_t x = byte_pad; x < in_width; x++) {
                acc_value = acc_value | (
                    (in[j + x] & ((bit_len_mask >> (8 * (in_width - x - 1))) & 0xFF))
                    << (8 * (in_width - x - 1)));
            }
            j += in_width;
            acc_bits += bit_len;
        }
        acc_bits -= 8;
        out[i] = (uint8_t)((acc_value >> acc_bits) & 0xFF);
    }
    (void)in_len;
}

std::vector<uint8_t> eh_compress_indices(const uint32_t indices[32]) {
    // 32 big-endian uint32 indices, then compress with bit_len = 17, byte_pad = 2
    // minLen = 17 * 128 / 32 = 68
    uint8_t array[32 * 4];
    for (int i = 0; i < 32; i++) store32_be(array + i * 4, indices[i]);
    std::vector<uint8_t> ret(68);
    // bit_len = collision_bit_len+1 = 17; byte_pad = 4 - ceil(17/8) = 1
    CompressArray(array, sizeof(array), ret.data(), 68, 17, 1);
    return ret;
}

bool eh_expand_indices(const uint8_t soln[68], uint32_t indices[32]) {
    uint8_t array[32 * 4];
    ExpandArray(soln, 68, array, sizeof(array), 17, 1);
    for (int i = 0; i < 32; i++) indices[i] = load32_be(array + i * 4);
    return true;
}

struct Row {
    uint8_t hash[12];
    uint8_t idx[128]; // up to 32 * 4
};

static bool has_collision(const Row& a, const Row& b, int l) {
    return std::memcmp(a.hash, b.hash, l) == 0;
}

static bool distinct_indices(const Row& a, const Row& b, size_t len, size_t lenIndices) {
    for (size_t i = 0; i < lenIndices; i += 4) {
        for (size_t j = 0; j < lenIndices; j += 4) {
            if (std::memcmp(a.idx + i, b.idx + j, 4) == 0) return false;
        }
    }
    (void)len;
    return true;
}

static bool indices_before(const Row& a, const Row& b, size_t lenIndices) {
    return std::memcmp(a.idx, b.idx, lenIndices) < 0;
}

static void merge_rows(Row& out, const Row& a, const Row& b, size_t hashLen, size_t lenIndices, int trim) {
    for (size_t i = (size_t)trim; i < hashLen; ++i)
        out.hash[i - (size_t)trim] = a.hash[i] ^ b.hash[i];
    if (indices_before(a, b, lenIndices)) {
        std::memcpy(out.idx, a.idx, lenIndices);
        std::memcpy(out.idx + lenIndices, b.idx, lenIndices);
    } else {
        std::memcpy(out.idx, b.idx, lenIndices);
        std::memcpy(out.idx + lenIndices, a.idx, lenIndices);
    }
}

static bool is_zero(const uint8_t* h, size_t len) {
    for (size_t i = 0; i < len; ++i) if (h[i]) return false;
    return true;
}

bool eh_is_valid_solution(const Blake2bState& base, const uint8_t soln[68]) {
    uint32_t indices[32];
    eh_expand_indices(soln, indices);

    Row X[32];
    uint8_t tmp[60];
    for (int n = 0; n < 32; ++n) {
        uint32_t i = indices[n];
        eh_generate_hash(base, i / EH_INDICES_PER_HASH, tmp);
        ExpandArray(tmp + (i % EH_INDICES_PER_HASH) * 12, 12, X[n].hash, 12, 16, 0);
        store32_be(X[n].idx, i);
    }

    size_t hashLen = 12;
    size_t lenIndices = 4;
    int count = 32;
    Row Xc[32];
    while (count > 1) {
        int nc = 0;
        for (int i = 0; i < count; i += 2) {
            if (!has_collision(X[i], X[i + 1], EH_COLLISION_BYTE_LEN)) return false;
            if (indices_before(X[i + 1], X[i], lenIndices)) return false;
            if (!distinct_indices(X[i], X[i + 1], hashLen, lenIndices)) return false;
            merge_rows(Xc[nc], X[i], X[i + 1], hashLen, lenIndices, EH_COLLISION_BYTE_LEN);
            nc++;
        }
        for (int i = 0; i < nc; ++i) X[i] = Xc[i];
        count = nc;
        hashLen -= EH_COLLISION_BYTE_LEN;
        lenIndices *= 2;
    }
    return is_zero(X[0].hash, hashLen);
}

int eh_solve_cpu(const Blake2bState& base,
                 const std::function<void(const EquihashSolution&)>& on_sol,
                 std::atomic<bool>* cancel) {
    const size_t Width = 12 + 4 * 16; // hash + 16 indices after round k-1; we keep growing idx
    std::vector<Row> X;
    X.resize(EH_INIT_SIZE);

    uint8_t tmp[60];
    size_t filled = 0;
    for (uint32_t g = 0; filled < EH_INIT_SIZE; ++g) {
        eh_generate_hash(base, g, tmp);
        for (int i = 0; i < EH_INDICES_PER_HASH && filled < EH_INIT_SIZE; ++i) {
            ExpandArray(tmp + i * 12, 12, X[filled].hash, 12, 16, 0);
            store32_be(X[filled].idx, (uint32_t)(g * EH_INDICES_PER_HASH + i));
            filled++;
        }
        if (cancel && cancel->load()) return 0;
    }

    size_t hashLen = 12;
    size_t lenIndices = 4;
    int found = 0;

    auto cancelled = [&]() { return cancel && cancel->load(); };

    for (int r = 1; r < EH_K && !X.empty(); ++r) {
        std::sort(X.begin(), X.end(), [&](const Row& a, const Row& b) {
            return std::memcmp(a.hash, b.hash, EH_COLLISION_BYTE_LEN) < 0;
        });
        if (cancelled()) return found;

        std::vector<Row> Xc;
        Xc.reserve(X.size());
        size_t i = 0;
        while (i + 1 < X.size()) {
            size_t j = 1;
            while (i + j < X.size() && has_collision(X[i], X[i + j], EH_COLLISION_BYTE_LEN)) j++;
            for (size_t l = 0; l + 1 < j; ++l) {
                for (size_t m = l + 1; m < j; ++m) {
                    if (distinct_indices(X[i + l], X[i + m], hashLen, lenIndices)) {
                        Row out{};
                        merge_rows(out, X[i + l], X[i + m], hashLen, lenIndices, EH_COLLISION_BYTE_LEN);
                        Xc.push_back(out);
                    }
                }
            }
            i += j;
        }
        X.swap(Xc);
        hashLen -= EH_COLLISION_BYTE_LEN;
        lenIndices *= 2;
        if (cancelled()) return found;
    }

    if (X.size() > 1) {
        std::sort(X.begin(), X.end(), [&](const Row& a, const Row& b) {
            return std::memcmp(a.hash, b.hash, hashLen) < 0;
        });
        size_t i = 0;
        while (i + 1 < X.size()) {
            size_t j = 1;
            while (i + j < X.size() && has_collision(X[i], X[i + j], (int)hashLen)) j++;
            for (size_t l = 0; l + 1 < j; ++l) {
                for (size_t m = l + 1; m < j; ++m) {
                    if (!distinct_indices(X[i + l], X[i + m], hashLen, lenIndices)) continue;
                    Row res{};
                    merge_rows(res, X[i + l], X[i + m], hashLen, lenIndices, 0);
                    if (!is_zero(res.hash, hashLen)) continue;
                    EquihashSolution sol;
                    sol.indices.resize(32);
                    for (int n = 0; n < 32; ++n) sol.indices[n] = load32_be(res.idx + n * 4);
                    sol.compressed = eh_compress_indices(sol.indices.data());
                    if (eh_is_valid_solution(base, sol.compressed.data())) {
                        on_sol(sol);
                        found++;
                    }
                }
            }
            i += j;
        }
    }
    (void)Width;
    return found;
}

void vds_build_pow_header(uint8_t out[281],
                          const uint8_t prefix[180],
                          const uint8_t nonce[32],
                          const uint8_t solution[68]) {
    std::memcpy(out, prefix, 180);
    std::memcpy(out + 180, nonce, 32);
    out[212] = 0x44; // compact size of 68
    std::memcpy(out + 213, solution, 68);
}

bool vds_check_pow(const uint8_t prefix[180],
                   const uint8_t nonce[32],
                   const uint8_t solution[68],
                   const uint8_t target[32],
                   uint8_t pow_hash_out[32]) {
    uint8_t hdr[281];
    vds_build_pow_header(hdr, prefix, nonce, solution);
    scrypt_1024_1_1_256(hdr, pow_hash_out);
    return uint256_leq(pow_hash_out, target);
}
