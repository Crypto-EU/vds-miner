#pragma once

#include <atomic>
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
    std::string version = "1.0.0";
};
