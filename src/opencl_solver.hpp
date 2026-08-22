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

    // empty device list = all AMD GPUs. pipes = concurrent Wagner solves per GPU (1 or 2).
    bool init(const std::vector<int>& device_indices, int pipes = 2);
    bool ready() const { return ready_; }
    int device_count() const { return (int)devices_.size(); }
    int pipes_per_device() const { return pipes_; }
    const std::vector<GpuDeviceInfo>& devices() const { return devices_; }

    // Full Equihash(96,5) solve on the GPU (hash + Wagner). CPU only validates.
    int solve(int dev, const uint8_t prefix[180], const uint8_t nonce[32],
              const std::function<void(const EquihashSolution&)>& on_sol,
              std::atomic<bool>* cancel = nullptr, int pipe = 0);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    std::vector<GpuDeviceInfo> devices_;
    bool ready_ = false;
    int pipes_ = 1;
};
