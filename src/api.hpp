#pragma once

#include "stats.hpp"
#include <string>
#include <thread>
#include <atomic>

// Tiny HTTP JSON stats server for HiveOS (default 127.0.0.1:4068).
class ApiServer {
public:
    ApiServer(MinerStats& stats, uint16_t port);
    ~ApiServer();
    void start();
    void stop();
    uint16_t port() const { return port_; }

private:
    void loop();
    MinerStats& stats_;
    uint16_t port_;
    int listen_fd_ = -1;
    std::atomic<bool> running_{false};
    std::thread thr_;
};

std::string stats_json(const MinerStats& st, uint64_t accepted, uint64_t rejected);
