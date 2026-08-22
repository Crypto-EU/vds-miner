#include "stratum.hpp"
#include "util.hpp"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <thread>

namespace {

int connect_tcp(const std::string& host, uint16_t port, int timeout_s) {
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* res = nullptr;
    std::string ps = std::to_string(port);
    if (getaddrinfo(host.c_str(), ps.c_str(), &hints, &res) != 0) return -1;
    int fd = -1;
    for (auto* p = res; p; p = p->ai_next) {
        fd = (int)socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (fd < 0) continue;
        timeval tv{};
        tv.tv_sec = timeout_s;
        setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
        int one = 1;
        setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
        if (connect(fd, p->ai_addr, p->ai_addrlen) == 0) break;
        close(fd);
        fd = -1;
    }
    freeaddrinfo(res);
    return fd;
}

// Extract a JSON-RPC method string.
std::string json_get_string(const std::string& j, const std::string& key) {
    std::string pat = "\"" + key + "\"";
    auto k = j.find(pat);
    if (k == std::string::npos) return {};
    auto c = j.find(':', k + pat.size());
    if (c == std::string::npos) return {};
    size_t i = c + 1;
    while (i < j.size() && (j[i] == ' ' || j[i] == '\t')) i++;
    if (i < j.size() && j[i] == '"') {
        i++;
        std::string out;
        while (i < j.size() && j[i] != '"') {
            if (j[i] == '\\' && i + 1 < j.size()) {
                out.push_back(j[i + 1]);
                i += 2;
                continue;
            }
            out.push_back(j[i++]);
        }
        return out;
    }
    return {};
}

int json_get_int(const std::string& j, const std::string& key, int def = 0) {
    std::string pat = "\"" + key + "\"";
    auto k = j.find(pat);
    if (k == std::string::npos) return def;
    auto c = j.find(':', k + pat.size());
    if (c == std::string::npos) return def;
    return (int)std::strtol(j.c_str() + c + 1, nullptr, 10);
}

bool json_get_bool_result(const std::string& j, bool& out) {
    auto k = j.find("\"result\"");
    if (k == std::string::npos) return false;
    auto c = j.find(':', k);
    if (c == std::string::npos) return false;
    size_t i = c + 1;
    while (i < j.size() && (j[i] == ' ' || j[i] == '\t')) i++;
    if (j.compare(i, 4, "true") == 0) { out = true; return true; }
    if (j.compare(i, 5, "false") == 0) { out = false; return true; }
    return false;
}

// Parse a JSON array of mixed strings/bools/nulls into strings ("true"/"false").
std::vector<std::string> json_array_at(const std::string& j, const std::string& key) {
    std::string pat = "\"" + key + "\"";
    auto k = j.find(pat);
    if (k == std::string::npos) return {};
    auto b = j.find('[', k);
    if (b == std::string::npos) return {};
    std::vector<std::string> out;
    size_t i = b + 1;
    while (i < j.size()) {
        while (i < j.size() && (j[i] == ' ' || j[i] == '\t' || j[i] == '\n' || j[i] == ',')) i++;
        if (i >= j.size() || j[i] == ']') break;
        if (j[i] == '"') {
            i++;
            std::string s;
            while (i < j.size() && j[i] != '"') {
                if (j[i] == '\\' && i + 1 < j.size()) { s.push_back(j[i + 1]); i += 2; continue; }
                s.push_back(j[i++]);
            }
            if (i < j.size() && j[i] == '"') i++;
            out.push_back(s);
        } else if (j.compare(i, 4, "true") == 0) {
            out.emplace_back("true"); i += 4;
        } else if (j.compare(i, 5, "false") == 0) {
            out.emplace_back("false"); i += 5;
        } else if (j.compare(i, 4, "null") == 0) {
            out.emplace_back(""); i += 4;
        } else {
            std::string s;
            while (i < j.size() && j[i] != ',' && j[i] != ']') s.push_back(j[i++]);
            while (!s.empty() && (s.back() == ' ' || s.back() == '\t')) s.pop_back();
            out.push_back(s);
        }
    }
    return out;
}

std::string json_error_reason(const std::string& j) {
    auto msg = json_get_string(j, "message");
    if (!msg.empty()) return msg;
    auto arr = json_array_at(j, "error");
    if (arr.size() > 1) return arr[1];
    if (!arr.empty()) return arr[0];
    auto s = json_get_string(j, "error");
    if (!s.empty()) return s;
    return "unknown";
}

std::string json_skip_ws(const std::string& j, size_t& i) {
    while (i < j.size() && (j[i] == ' ' || j[i] == '\t' || j[i] == '\n' || j[i] == '\r')) i++;
    return {};
}

// mining.subscribe result: [ <session-id or array>, "<extranonce1>", ... ]
std::string json_subscribe_extranonce(const std::string& j) {
    auto k = j.find("\"result\"");
    if (k == std::string::npos) return {};
    auto b = j.find('[', k);
    if (b == std::string::npos) return {};
    size_t i = b + 1;
    json_skip_ws(j, i);
    // skip first element
    if (i < j.size() && j[i] == '[') {
        int depth = 0;
        do {
            if (j[i] == '[') depth++;
            else if (j[i] == ']') depth--;
            i++;
        } while (i < j.size() && depth > 0);
    } else if (i < j.size() && j[i] == '"') {
        i++;
        while (i < j.size() && j[i] != '"') {
            if (j[i] == '\\') i++;
            i++;
        }
        if (i < j.size()) i++;
    } else {
        while (i < j.size() && j[i] != ',') i++;
    }
    while (i < j.size() && j[i] != ',') i++;
    if (i < j.size() && j[i] == ',') i++;
    json_skip_ws(j, i);
    if (i < j.size() && j[i] == '"') {
        i++;
        std::string s;
        while (i < j.size() && j[i] != '"') {
            if (j[i] == '\\' && i + 1 < j.size()) { s.push_back(j[i + 1]); i += 2; continue; }
            s.push_back(j[i++]);
        }
        return s;
    }
    return {};
}

} // namespace

StratumClient::StratumClient() = default;
StratumClient::~StratumClient() { stop(); }

void StratumClient::set_endpoint(std::string host, uint16_t port, std::string user, std::string pass) {
    host_ = std::move(host);
    port_ = port;
    user_ = std::move(user);
    pass_ = std::move(pass);
}

void StratumClient::start() {
    if (running_.exchange(true)) return;
    thr_ = std::thread([this] { io_loop(); });
}

void StratumClient::stop() {
    running_ = false;
    if (sock_ >= 0) {
        shutdown(sock_, SHUT_RDWR);
        close(sock_);
        sock_ = -1;
    }
    if (thr_.joinable()) thr_.join();
}

bool StratumClient::current_job(StratumJob& out) const {
    std::lock_guard<std::mutex> g(mu_);
    if (job_.job_id.empty()) return false;
    out = job_;
    return true;
}

bool StratumClient::send_line(const std::string& s) {
    if (sock_ < 0) return false;
    std::string line = s;
    if (line.empty() || line.back() != '\n') line.push_back('\n');
    size_t sent = 0;
    while (sent < line.size()) {
        ssize_t n = ::send(sock_, line.data() + sent, line.size() - sent, 0);
        if (n <= 0) return false;
        sent += (size_t)n;
    }
    LOGD(">> %s", s.c_str());
    return true;
}

bool StratumClient::submit(const StratumJob& job, const uint8_t nonce[32], const uint8_t solution[68]) {
    // nheqminer: serialize nonce||solution, then nonce2 = hex of nonce after nonce1 hex prefix,
    // solution field = remaining hex including compact-size.
    std::string nonce_hex = to_hex(nonce, 32);
    std::string sol_hex = "44" + to_hex(solution, 68);
    size_t n1hex = extra_nonce1_hex_.size();
    if (n1hex > 64) n1hex = 64;
    std::string nonce2 = nonce_hex.substr(n1hex);
    int id = next_id_++;
    std::ostringstream os;
    os << "{\"id\":" << id
       << ",\"method\":\"mining.submit\",\"params\":[\""
       << user_ << "\",\"" << job.job_id << "\",\"" << job.ntime_hex
       << "\",\"" << nonce2 << "\",\"" << sol_hex << "\"]}";
    return send_line(os.str());
}

void StratumClient::apply_notify(const std::vector<std::string>& params, bool clean) {
    if (params.size() < 11) {
        LOGW("mining.notify mit zu wenigen Parametern (%zu)", params.size());
        return;
    }
    // Concatenate version..utxo_root as serialized header prefix (180 bytes).
    std::string hex;
    for (int i = 1; i <= 9; ++i) hex += params[i];
    std::vector<uint8_t> prefix;
    try {
        prefix = parse_hex(hex);
    } catch (...) {
        LOGE("Ungueltiger Header in mining.notify");
        return;
    }
    if (prefix.size() != 180) {
        LOGW("Header-Prefix Laenge %zu (erwartet 180)", prefix.size());
        if (prefix.size() < 180) return;
        prefix.resize(180);
    }
    StratumJob job;
    job.job_id = params[0];
    job.ntime_hex = params[6];
    std::memcpy(job.header_prefix, prefix.data(), 180);
    job.nonce1_bytes = extra_nonce1_hex_.size() / 2;
    std::memset(job.nonce1, 0, 32);
    try {
        auto n1 = parse_hex(extra_nonce1_hex_);
        if (n1.size() > 32) n1.resize(32);
        std::memcpy(job.nonce1, n1.data(), n1.size());
        job.nonce1_bytes = n1.size();
    } catch (...) {}
    uint256_from_hex_be(pending_target_hex_, job.target);
    job.clean = clean;
    job.job_epoch = job_epoch_.fetch_add(1) + 1;
    {
        std::lock_guard<std::mutex> g(mu_);
        job_ = job;
    }
    double need = uint256_expected_hashes(job.target);
    LOGI("Neues Job #%s  nTime=%s", job.job_id.c_str(), job.ntime_hex.c_str());
    LOGI("  Target %s  (~%.1f Mio. Loesungen/Share)",
         uint256_to_hex_be(job.target).c_str(), need / 1e6);
}

void StratumClient::handle_line(const std::string& line) {
    LOGD("<< %s", line.c_str());
    std::string method = json_get_string(line, "method");
    int id = json_get_int(line, "id", 0);

    if (method == "mining.notify") {
        auto params = json_array_at(line, "params");
        bool clean = true;
        if (params.size() > 10) clean = (params[10] != "false");
        if (!clean) {
            LOGD("Ignoriere non-clean Job");
            return;
        }
        apply_notify(params, clean);
        return;
    }
    if (method == "mining.set_target") {
        auto params = json_array_at(line, "params");
        if (!params.empty()) {
            pending_target_hex_ = params[0];
            uint8_t t[32]{};
            uint256_from_hex_be(pending_target_hex_, t);
            LOGI("Target gesetzt: %s  (~%.1f Mio. Loesungen/Share)",
                 pending_target_hex_.c_str(), uint256_expected_hashes(t) / 1e6);
            std::lock_guard<std::mutex> g(mu_);
            if (!job_.job_id.empty()) std::memcpy(job_.target, t, 32);
        }
        return;
    }
    if (method == "mining.set_extranonce") {
        auto params = json_array_at(line, "params");
        if (!params.empty()) {
            extra_nonce1_hex_ = params[0];
            LOGI("Extranonce: %s", extra_nonce1_hex_.c_str());
        }
        return;
    }

    if (id == 1) {
        extra_nonce1_hex_ = json_subscribe_extranonce(line);
        if (extra_nonce1_hex_.empty()) {
            auto result = json_array_at(line, "result");
            if (result.size() >= 2) extra_nonce1_hex_ = result[1];
        }
        if (!extra_nonce1_hex_.empty()) {
            LOGI("Subscribe OK, extranonce1=%s", extra_nonce1_hex_.c_str());
            std::ostringstream os;
            os << "{\"id\":2,\"method\":\"mining.authorize\",\"params\":[\""
               << user_ << "\",\"" << pass_ << "\"]}";
            send_line(os.str());
        } else {
            LOGE("Subscribe fehlgeschlagen: %s", line.c_str());
        }
        return;
    }
    if (id == 2) {
        bool ok = false;
        json_get_bool_result(line, ok);
        if (!ok) {
            LOGE("Authorize fehlgeschlagen fuer %s: %s", user_.c_str(), json_error_reason(line).c_str());
            authorized_ = false;
            return;
        }
        authorized_ = true;
        LOGI("Worker autorisiert: %s", user_.c_str());
        send_line("{\"id\":3,\"method\":\"mining.extranonce.subscribe\",\"params\":[]}");
        return;
    }
    if (id >= 4) {
        bool ok = false;
        if (json_get_bool_result(line, ok) && ok) {
            accepted_++;
            LOGI("Share akzeptiert  (A:%llu R:%llu)",
                 (unsigned long long)accepted_.load(), (unsigned long long)rejected_.load());
        } else {
            rejected_++;
            LOGW("Share abgelehnt: %s  (A:%llu R:%llu)",
                 json_error_reason(line).c_str(),
                 (unsigned long long)accepted_.load(), (unsigned long long)rejected_.load());
        }
    }
}

void StratumClient::reconnect() {
    if (sock_ >= 0) { close(sock_); sock_ = -1; }
    connected_ = false;
    authorized_ = false;
    recv_buf_.clear();
}

void StratumClient::io_loop() {
    while (running_) {
        LOGI("Verbinde zu %s:%u ...", host_.c_str(), port_);
        sock_ = connect_tcp(host_, port_, 15);
        if (sock_ < 0) {
            LOGE("Verbindung fehlgeschlagen, neuer Versuch in 5s");
            for (int i = 0; i < 50 && running_; ++i) std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }
        connected_ = true;
        LOGI("TCP verbunden");
        std::ostringstream os;
        os << "{\"id\":1,\"method\":\"mining.subscribe\",\"params\":[\"vds-miner/1.1.2\",null,\""
           << host_ << "\",\"" << port_ << "\"]}";
        if (!send_line(os.str())) {
            reconnect();
            continue;
        }

        char buf[4096];
        while (running_ && sock_ >= 0) {
            ssize_t n = ::recv(sock_, buf, sizeof(buf), 0);
            if (n == 0) {
                LOGW("Pool hat die Verbindung getrennt");
                break;
            }
            if (n < 0) {
                if (!running_) break;
                // timeout: keep waiting
                continue;
            }
            recv_buf_.append(buf, (size_t)n);
            size_t pos;
            while ((pos = recv_buf_.find('\n')) != std::string::npos) {
                std::string line = recv_buf_.substr(0, pos);
                recv_buf_.erase(0, pos + 1);
                if (!line.empty() && line.back() == '\r') line.pop_back();
                if (!line.empty()) {
                    try { handle_line(line); }
                    catch (const std::exception& e) { LOGW("Nachricht ignoriert: %s", e.what()); }
                }
            }
        }
        reconnect();
        if (running_) {
            LOGI("Reconnect in 3s");
            std::this_thread::sleep_for(std::chrono::seconds(3));
        }
    }
}
