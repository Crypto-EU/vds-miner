#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <functional>
#include <thread>
#include <condition_variable>

struct StratumJob {
    std::string job_id;
    std::string ntime_hex;
    uint8_t header_prefix[180]{};
    uint8_t nonce1[32]{};
    size_t nonce1_bytes = 0;
    uint8_t target[32]{};
    uint64_t job_epoch = 0;
    bool clean = true;
};

class StratumClient {
public:
    using SubmitFn = std::function<void(bool accepted, const std::string& reason)>;

    StratumClient();
    ~StratumClient();

    void set_endpoint(std::string host, uint16_t port, std::string user, std::string pass);
    void start();
    void stop();

    bool connected() const { return connected_.load(); }
    bool authorized() const { return authorized_.load(); }

    // Copy current job if one is available.
    bool current_job(StratumJob& out) const;

    // nonce2 is the remaining bytes after nonce1 (big-endian hex as used by nheqminer).
    bool submit(const StratumJob& job, const uint8_t nonce[32], const uint8_t solution[68]);

    uint64_t accepted() const { return accepted_.load(); }
    uint64_t rejected() const { return rejected_.load(); }
    uint64_t job_epoch() const { return job_epoch_.load(); }

private:
    void io_loop();
    void reconnect();
    bool send_line(const std::string& s);
    void handle_line(const std::string& line);
    void apply_notify(const std::vector<std::string>& params, bool clean);

    std::string host_, user_, pass_;
    uint16_t port_ = 9338;
    int sock_ = -1;
    std::atomic<bool> running_{false};
    std::atomic<bool> connected_{false};
    std::atomic<bool> authorized_{false};
    std::thread thr_;
    mutable std::mutex mu_;
    StratumJob job_;
    std::atomic<uint64_t> job_epoch_{0};
    std::atomic<uint64_t> accepted_{0};
    std::atomic<uint64_t> rejected_{0};
    int next_id_ = 4;
    std::string extra_nonce1_hex_;
    std::string pending_target_hex_ = "0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f";
    std::string recv_buf_;
};
