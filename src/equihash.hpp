#pragma once

#include <cstdint>
#include <vector>
#include <functional>
#include <atomic>

#include "crypto.hpp"

// VDS Equihash(n=96, k=5). Compressed solution is 68 bytes (plus 1-byte compact size = 69).
constexpr int EH_N = 96;
constexpr int EH_K = 5;
constexpr int EH_COLLISION_BIT_LEN = 16;
constexpr int EH_COLLISION_BYTE_LEN = 2;
constexpr int EH_HASH_LEN = 12;
constexpr int EH_INDICES = 32;           // 2^k
constexpr int EH_INIT_SIZE = 131072;     // 2^(collision_bit_len+1)
constexpr int EH_INDICES_PER_HASH = 5;   // 512/N
constexpr int EH_HASH_OUTPUT = 60;       // 5 * 12
constexpr int EH_SOLUTION_SIZE = 68;     // (1<<k)*(collision_bit_len+1)/8
constexpr int EH_HEADER_PREFIX = 180;    // header without nonce/solution
constexpr int EH_NONCE_SIZE = 32;
constexpr int EH_POW_HEADER = 281;       // 212 + 1 + 68

struct EquihashSolution {
    std::vector<uint8_t> compressed; // 68 bytes
    std::vector<uint32_t> indices;   // 32 indices, canonical order
};

void eh_init_state(Blake2bState* S, const uint8_t header_prefix[180], const uint8_t nonce[32]);
void eh_generate_hash(const Blake2bState& base, uint32_t g, uint8_t out[60]);

bool eh_is_valid_solution(const Blake2bState& base, const uint8_t soln[68]);
std::vector<uint8_t> eh_compress_indices(const uint32_t indices[32]);
bool eh_expand_indices(const uint8_t soln[68], uint32_t indices[32]);

// CPU Wagner solver. Calls on_sol for every valid compressed 68-byte solution.
// Returns number of solutions found. cancel is checked between rounds.
int eh_solve_cpu(const Blake2bState& base,
                 const std::function<void(const EquihashSolution&)>& on_sol,
                 std::atomic<bool>* cancel = nullptr);

// Build the 281-byte VDS PoW header: prefix(180) + nonce(32) + 0x44 + solution(68)
void vds_build_pow_header(uint8_t out[281],
                          const uint8_t prefix[180],
                          const uint8_t nonce[32],
                          const uint8_t solution[68]);

bool vds_check_pow(const uint8_t prefix[180],
                   const uint8_t nonce[32],
                   const uint8_t solution[68],
                   const uint8_t target[32],
                   uint8_t pow_hash_out[32]);
