#pragma once

#include "util.hpp"

#include <atomic>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>
#include <cstdint>

struct GpuStats {
    std::string name;
    double sols_per_s = 0;
    uint64_t solutions = 0;
    int temp = 0;
    int fan = 0;
    int bus = -1;
};

struct MinerStats {
    std::atomic<uint64_t> hashes{0};
    std::atomic<uint64_t> solutions{0};
    std::atomic<uint64_t> shares_found{0};
    std::atomic<uint64_t> accepted{0};
    std::atomic<uint64_t> rejected{0};
    uint64_t start_ms = 0;
    mutable std::mutex mu;
    std::vector<GpuStats> gpus;
    std::string version = "1.1.3";
    uint8_t best_pow[32]{};
    bool best_pow_set = false;

    // Returns true if h is the best (lowest) PoW hash seen so far.
    bool consider_pow(const uint8_t h[32]) {
        std::lock_guard<std::mutex> g(mu);
        if (best_pow_set && !uint256_lt(h, best_pow)) return false;
        std::memcpy(best_pow, h, 32);
        best_pow_set = true;
        return true;
    }
};
