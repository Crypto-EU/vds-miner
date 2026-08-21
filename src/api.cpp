#include "api.hpp"
#include "util.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <sstream>
#include <thread>

std::string stats_json(const MinerStats& st, uint64_t accepted, uint64_t rejected) {
    uint64_t elapsed_ms = now_ms() - st.start_ms;
    if (elapsed_ms == 0) elapsed_ms = 1;
    double total_hs = (double)st.hashes.load() * 1000.0 / (double)elapsed_ms;
    // Equihash "hash" is one nonce attempt; report Sol/s as solutions/sec
    double total_sols = (double)st.solutions.load() * 1000.0 / (double)elapsed_ms;

    std::lock_guard<std::mutex> g(st.mu);
    std::ostringstream hs, temp, fan, bus;
    hs << "["; temp << "["; fan << "["; bus << "[";
    for (size_t i = 0; i < st.gpus.size(); ++i) {
        if (i) { hs << ","; temp << ","; fan << ","; bus << ","; }
        double v = st.gpus[i].sols_per_s > 0 ? st.gpus[i].sols_per_s : (st.gpus.size() ? total_sols / st.gpus.size() : total_sols);
        hs << v;
        temp << st.gpus[i].temp;
        fan << st.gpus[i].fan;
        bus << (st.gpus[i].bus >= 0 ? st.gpus[i].bus : (int)i);
    }
    if (st.gpus.empty()) {
        hs << total_sols;
        temp << 0; fan << 0; bus << 0;
    }
    hs << "]"; temp << "]"; fan << "]"; bus << "]";

    std::ostringstream os;
    os << "{"
       << "\"hs\":" << hs.str() << ","
       << "\"hs_units\":\"hs\","
       << "\"temp\":" << temp.str() << ","
       << "\"fan\":" << fan.str() << ","
       << "\"uptime\":" << (elapsed_ms / 1000) << ","
       << "\"ver\":\"" << st.version << "\","
       << "\"ar\":[" << accepted << "," << rejected << "],"
       << "\"algo\":\"equihash96_5\","
       << "\"bus_numbers\":" << bus.str() << ","
       << "\"khs\":" << (total_sols / 1000.0) << ","
       << "\"sols\":" << total_sols << ","
       << "\"iters\":" << total_hs
       << "}";
    return os.str();
}

ApiServer::ApiServer(MinerStats& stats, uint16_t port) : stats_(stats), port_(port) {}
ApiServer::~ApiServer() { stop(); }

void ApiServer::start() {
    listen_fd_ = (int)socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd_ < 0) {
        LOGE("API-Socket fehlgeschlagen");
        return;
    }
    int one = 1;
    setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_port = htons(port_);
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (bind(listen_fd_, (sockaddr*)&a, sizeof(a)) < 0) {
        LOGW("API-Port %u belegt, versuche automatisch", port_);
        a.sin_port = 0;
        if (bind(listen_fd_, (sockaddr*)&a, sizeof(a)) < 0) {
            LOGE("API bind fehlgeschlagen");
            close(listen_fd_); listen_fd_ = -1; return;
        }
        socklen_t sl = sizeof(a);
        getsockname(listen_fd_, (sockaddr*)&a, &sl);
        port_ = ntohs(a.sin_port);
    }
    listen(listen_fd_, 8);
    running_ = true;
    thr_ = std::thread([this] { loop(); });
    LOGI("API lauscht auf 127.0.0.1:%u", port_);
}

void ApiServer::stop() {
    running_ = false;
    if (listen_fd_ >= 0) { shutdown(listen_fd_, SHUT_RDWR); close(listen_fd_); listen_fd_ = -1; }
    if (thr_.joinable()) thr_.join();
}

void ApiServer::loop() {
    while (running_) {
        int c = accept(listen_fd_, nullptr, nullptr);
        if (c < 0) continue;
        char req[1024];
        recv(c, req, sizeof(req), 0);
        std::string body = stats_json(stats_, stats_.accepted.load(), stats_.rejected.load());
        // accepted/rejected filled by caller via MinerStats? use zeros here; main overwrites through stats_json in h-stats.
        std::ostringstream resp;
        resp << "HTTP/1.1 200 OK\r\n"
             << "Content-Type: application/json\r\n"
             << "Connection: close\r\n"
             << "Content-Length: " << body.size() << "\r\n\r\n"
             << body;
        auto s = resp.str();
        send(c, s.data(), s.size(), 0);
        close(c);
    }
}
