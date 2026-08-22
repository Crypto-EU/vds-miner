#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <stdexcept>
#include <chrono>
#include <mutex>
#include <cstdio>
#include <cstdarg>
#include <sstream>
#include <iomanip>
#include <cmath>

inline uint32_t load32_le(const void* p) {
    const auto* b = static_cast<const uint8_t*>(p);
    return (uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
}

inline uint64_t load64_le(const void* p) {
    const auto* b = static_cast<const uint8_t*>(p);
    return (uint64_t)b[0] | ((uint64_t)b[1] << 8) | ((uint64_t)b[2] << 16) | ((uint64_t)b[3] << 24)
         | ((uint64_t)b[4] << 32) | ((uint64_t)b[5] << 40) | ((uint64_t)b[6] << 48) | ((uint64_t)b[7] << 56);
}

inline void store32_le(void* p, uint32_t x) {
    auto* b = static_cast<uint8_t*>(p);
    b[0] = (uint8_t)x; b[1] = (uint8_t)(x >> 8); b[2] = (uint8_t)(x >> 16); b[3] = (uint8_t)(x >> 24);
}

inline void store64_le(void* p, uint64_t x) {
    auto* b = static_cast<uint8_t*>(p);
    b[0] = (uint8_t)x; b[1] = (uint8_t)(x >> 8); b[2] = (uint8_t)(x >> 16); b[3] = (uint8_t)(x >> 24);
    b[4] = (uint8_t)(x >> 32); b[5] = (uint8_t)(x >> 40); b[6] = (uint8_t)(x >> 48); b[7] = (uint8_t)(x >> 56);
}

inline uint32_t load32_be(const void* p) {
    const auto* b = static_cast<const uint8_t*>(p);
    return ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16) | ((uint32_t)b[2] << 8) | (uint32_t)b[3];
}

inline void store32_be(void* p, uint32_t x) {
    auto* b = static_cast<uint8_t*>(p);
    b[0] = (uint8_t)(x >> 24); b[1] = (uint8_t)(x >> 16); b[2] = (uint8_t)(x >> 8); b[3] = (uint8_t)x;
}

inline int hex_nibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

inline std::vector<uint8_t> parse_hex(const std::string& hex) {
    if (hex.size() % 2) throw std::runtime_error("odd hex length");
    std::vector<uint8_t> out(hex.size() / 2);
    for (size_t i = 0; i < out.size(); ++i) {
        int hi = hex_nibble(hex[2 * i]);
        int lo = hex_nibble(hex[2 * i + 1]);
        if (hi < 0 || lo < 0) throw std::runtime_error("invalid hex");
        out[i] = (uint8_t)((hi << 4) | lo);
    }
    return out;
}

inline std::string to_hex(const uint8_t* p, size_t n) {
    static const char* k = "0123456789abcdef";
    std::string s(n * 2, '0');
    for (size_t i = 0; i < n; ++i) {
        s[2 * i] = k[p[i] >> 4];
        s[2 * i + 1] = k[p[i] & 0xf];
    }
    return s;
}

inline std::string to_hex(const std::vector<uint8_t>& v) {
    return to_hex(v.data(), v.size());
}

// Bitcoin-style uint256: hex string is MSB-first; memory is LSB-first.
inline void uint256_from_hex_be(const std::string& hex, uint8_t out[32]) {
    std::string h = hex;
    if (h.size() >= 2 && h[0] == '0' && (h[1] == 'x' || h[1] == 'X')) h = h.substr(2);
    while (h.size() < 64) h.insert(h.begin(), '0');
    if (h.size() > 64) h = h.substr(h.size() - 64);
    auto bytes = parse_hex(h);
    for (int i = 0; i < 32; ++i) out[i] = bytes[31 - i];
}

inline std::string uint256_to_hex_be(const uint8_t in[32]) {
    uint8_t be[32];
    for (int i = 0; i < 32; ++i) be[i] = in[31 - i];
    return to_hex(be, 32);
}

// Compare two 256-bit little-endian numbers. Returns true if a <= b.
inline bool uint256_leq(const uint8_t a[32], const uint8_t b[32]) {
    for (int i = 31; i >= 0; --i) {
        if (a[i] < b[i]) return true;
        if (a[i] > b[i]) return false;
    }
    return true;
}

inline bool uint256_lt(const uint8_t a[32], const uint8_t b[32]) {
    for (int i = 31; i >= 0; --i) {
        if (a[i] < b[i]) return true;
        if (a[i] > b[i]) return false;
    }
    return false;
}

// Mean Equihash solutions needed for one share: 2^256 / target.
inline double uint256_expected_hashes(const uint8_t target_le[32]) {
    int i = 31;
    while (i >= 0 && target_le[i] == 0) --i;
    if (i < 0) return 1e300;
    double t = (double)target_le[i];
    if (i > 0) t += (double)target_le[i - 1] / 256.0;
    if (i > 1) t += (double)target_le[i - 2] / 65536.0;
    if (t <= 0.0) return 1e300;
    return std::ldexp(1.0, 8 * (32 - i)) / t;
}

// Anzeige: 1000 Equihash-Loesungen/s = 1 MH/s, damit HiveOS Megahash zeigt
// (sonst waeren 1760 Sol/s nur 0.002 MH/s und das Dashboard zeigt 0.00).
constexpr double kSolsPerMhs = 1000.0;
inline double sols_to_mhs(double sols_per_s) { return sols_per_s / kSolsPerMhs; }

inline std::string format_duration_s(double seconds) {
    if (!(seconds > 0) || seconds > 1e12) return "?";
    char b[48];
    if (seconds < 90) snprintf(b, sizeof(b), "%.0f s", seconds);
    else if (seconds < 7200) snprintf(b, sizeof(b), "%.0f min", seconds / 60.0);
    else snprintf(b, sizeof(b), "%.1f h", seconds / 3600.0);
    return b;
}

inline uint64_t now_ms() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

inline uint64_t unix_s() {
    using namespace std::chrono;
    return duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
}

class Log {
public:
    enum Level { TRACE = 0, DEBUG = 1, INFO = 2, WARN = 3, ERROR = 4 };
    static Log& instance() {
        static Log l;
        return l;
    }
    void set_level(Level lv) { level_ = lv; }

    void write(Level lv, const char* fmt, ...) {
        if (lv < level_) return;
        char buf[2048];
        va_list ap;
        va_start(ap, fmt);
        vsnprintf(buf, sizeof(buf), fmt, ap);
        va_end(ap);
        const char* tag = "I";
        if (lv == TRACE) tag = "T";
        else if (lv == DEBUG) tag = "D";
        else if (lv == WARN) tag = "W";
        else if (lv == ERROR) tag = "E";
        std::lock_guard<std::mutex> g(mu_);
        std::fprintf(stderr, "[%s] %s\n", tag, buf);
        std::fflush(stderr);
    }

private:
    std::mutex mu_;
    Level level_ = INFO;
};

#define LOGI(...) Log::instance().write(Log::INFO, __VA_ARGS__)
#define LOGW(...) Log::instance().write(Log::WARN, __VA_ARGS__)
#define LOGE(...) Log::instance().write(Log::ERROR, __VA_ARGS__)
#define LOGD(...) Log::instance().write(Log::DEBUG, __VA_ARGS__)
