#pragma once

#include "equihash.hpp"

#include <string>
#include <vector>
#include <atomic>
#include <functional>
#include <memory>

struct GpuDeviceInfo {
    int index = -1;
    std::string name;
    std::string vendor;
    std::string version;
    uint64_t global_mem = 0;
    uint32_t compute_units = 0;
    bool amd = false;
    std::string board_hint; // "6800xt" / "5700xt" / other
};

class OpenClSolver {
public:
    static bool available();
    static std::vector<GpuDeviceInfo> list_devices();

    OpenClSolver();
    ~OpenClSolver();

    bool init(const std::vector<int>& device_indices); // empty = all AMD GPUs
    bool ready() const { return ready_; }
    int device_count() const { return (int)devices_.size(); }
    const std::vector<GpuDeviceInfo>& devices() const { return devices_; }

    // Generate 131072 x 12-byte hashes for header||nonce on device `dev`.
    // Returns false on OpenCL error (caller should fall back to CPU).
    bool generate_hashes(int dev, const uint8_t prefix[180], const uint8_t nonce[32],
                         std::vector<uint8_t>& hashes_out);

    // Full solve: GPU hash generation + CPU Wagner. Thread-safe per device.
    int solve(int dev, const uint8_t prefix[180], const uint8_t nonce[32],
              const std::function<void(const EquihashSolution&)>& on_sol,
              std::atomic<bool>* cancel = nullptr);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    std::vector<GpuDeviceInfo> devices_;
    bool ready_ = false;
};

// CPU-only hash generation + Wagner (used when OpenCL is missing).
int eh_solve_from_header(const uint8_t prefix[180], const uint8_t nonce[32],
                         const std::function<void(const EquihashSolution&)>& on_sol,
                         std::atomic<bool>* cancel = nullptr);
