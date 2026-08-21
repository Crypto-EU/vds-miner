#include "opencl_solver.hpp"
#include "util.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <memory>
#include <mutex>
#include <sstream>

#ifdef __APPLE__
#include <OpenCL/opencl.h>
#else
#define CL_TARGET_OPENCL_VERSION 120
#include <CL/cl.h>
#endif

namespace {

const char* kKernelSource = R"CLC(
#define EH_INIT_SIZE 131072
#define EH_INDICES_PER_HASH 5
inline ulong rotr64(ulong x, uint n) { return (x >> n) | (x << (64 - n)); }
constant ulong BLAKE2B_IV[8] = {
    0x6A09E667F3BCC908UL, 0xBB67AE8584CAA73BUL,
    0x3C6EF372FE94F82BUL, 0xA54FF53A5F1D36F1UL,
    0x510E527FADE682D1UL, 0x9B05688C2B3E6C1FUL,
    0x1F83D9ABFB41BD6BUL, 0x5BE0CD19137E2179UL
};
constant uchar BLAKE2B_SIGMA[12][16] = {
    { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 },
    { 14, 10, 4, 8, 9, 15, 13, 6, 1, 12, 0, 2, 11, 7, 5, 3 },
    { 11, 8, 12, 0, 5, 2, 15, 13, 10, 14, 3, 6, 7, 1, 9, 4 },
    { 7, 9, 3, 1, 13, 12, 11, 14, 2, 6, 5, 10, 4, 0, 15, 8 },
    { 9, 0, 5, 7, 2, 4, 10, 15, 14, 1, 11, 12, 6, 8, 3, 13 },
    { 2, 12, 6, 10, 0, 11, 8, 3, 4, 13, 7, 5, 15, 14, 1, 9 },
    { 12, 5, 1, 15, 14, 13, 4, 10, 0, 7, 6, 3, 9, 2, 8, 11 },
    { 13, 11, 7, 14, 12, 1, 3, 9, 5, 0, 15, 4, 8, 6, 2, 10 },
    { 6, 15, 14, 9, 11, 3, 0, 8, 12, 2, 13, 7, 1, 4, 10, 5 },
    { 10, 2, 8, 4, 7, 6, 1, 5, 15, 11, 9, 14, 3, 12, 13, 0 },
    { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 },
    { 14, 10, 4, 8, 9, 15, 13, 6, 1, 12, 0, 2, 11, 7, 5, 3 }
};
void blake2b_g(ulong v[16], int a, int b, int c, int d, ulong x, ulong y) {
    v[a] = v[a] + v[b] + x; v[d] = rotr64(v[d] ^ v[a], 32);
    v[c] = v[c] + v[d]; v[b] = rotr64(v[b] ^ v[c], 24);
    v[a] = v[a] + v[b] + y; v[d] = rotr64(v[d] ^ v[a], 16);
    v[c] = v[c] + v[d]; v[b] = rotr64(v[b] ^ v[c], 63);
}
void blake2b_compress(ulong h[8], ulong t0, ulong t1, const uchar block[128], int last) {
    ulong m[16], v[16];
    for (int i = 0; i < 16; ++i) {
        int o = i * 8;
        m[i] = (ulong)block[o] | ((ulong)block[o+1] << 8) | ((ulong)block[o+2] << 16) | ((ulong)block[o+3] << 24)
             | ((ulong)block[o+4] << 32) | ((ulong)block[o+5] << 40) | ((ulong)block[o+6] << 48) | ((ulong)block[o+7] << 56);
    }
    for (int i = 0; i < 8; ++i) v[i] = h[i];
    v[8] = BLAKE2B_IV[0]; v[9] = BLAKE2B_IV[1]; v[10] = BLAKE2B_IV[2]; v[11] = BLAKE2B_IV[3];
    v[12] = BLAKE2B_IV[4] ^ t0; v[13] = BLAKE2B_IV[5] ^ t1;
    v[14] = BLAKE2B_IV[6] ^ (last ? 0xFFFFFFFFFFFFFFFFUL : 0); v[15] = BLAKE2B_IV[7];
    for (int r = 0; r < 12; ++r) {
        blake2b_g(v, 0, 4, 8, 12, m[BLAKE2B_SIGMA[r][0]], m[BLAKE2B_SIGMA[r][1]]);
        blake2b_g(v, 1, 5, 9, 13, m[BLAKE2B_SIGMA[r][2]], m[BLAKE2B_SIGMA[r][3]]);
        blake2b_g(v, 2, 6, 10, 14, m[BLAKE2B_SIGMA[r][4]], m[BLAKE2B_SIGMA[r][5]]);
        blake2b_g(v, 3, 7, 11, 15, m[BLAKE2B_SIGMA[r][6]], m[BLAKE2B_SIGMA[r][7]]);
        blake2b_g(v, 0, 5, 10, 15, m[BLAKE2B_SIGMA[r][8]], m[BLAKE2B_SIGMA[r][9]]);
        blake2b_g(v, 1, 6, 11, 12, m[BLAKE2B_SIGMA[r][10]], m[BLAKE2B_SIGMA[r][11]]);
        blake2b_g(v, 2, 7, 8, 13, m[BLAKE2B_SIGMA[r][12]], m[BLAKE2B_SIGMA[r][13]]);
        blake2b_g(v, 3, 4, 9, 14, m[BLAKE2B_SIGMA[r][14]], m[BLAKE2B_SIGMA[r][15]]);
    }
    for (int i = 0; i < 8; ++i) h[i] ^= v[i] ^ v[i + 8];
}
__kernel void generate_hashes(
    __global const ulong *h_in, const ulong t0, const ulong t1,
    __global const uchar *tail, const uint tail_len,
    __global uchar *out_hashes) {
    uint g = get_global_id(0);
    const uint max_g = (EH_INIT_SIZE + EH_INDICES_PER_HASH - 1) / EH_INDICES_PER_HASH;
    if (g >= max_g) return;
    ulong h[8];
    for (int i = 0; i < 8; ++i) h[i] = h_in[i];
    uchar block[128];
    for (uint i = 0; i < 128; ++i) block[i] = 0;
    for (uint i = 0; i < tail_len; ++i) block[i] = tail[i];
    block[tail_len + 0] = (uchar)(g);
    block[tail_len + 1] = (uchar)(g >> 8);
    block[tail_len + 2] = (uchar)(g >> 16);
    block[tail_len + 3] = (uchar)(g >> 24);
    ulong nt0 = t0 + (tail_len + 4);
    ulong nt1 = t1;
    if (nt0 < t0) nt1++;
    blake2b_compress(h, nt0, nt1, block, 1);
    uchar tmp[64];
    for (int i = 0; i < 8; ++i) {
        ulong v = h[i]; int o = i * 8;
        tmp[o+0]=(uchar)v; tmp[o+1]=(uchar)(v>>8); tmp[o+2]=(uchar)(v>>16); tmp[o+3]=(uchar)(v>>24);
        tmp[o+4]=(uchar)(v>>32); tmp[o+5]=(uchar)(v>>40); tmp[o+6]=(uchar)(v>>48); tmp[o+7]=(uchar)(v>>56);
    }
    for (int i = 0; i < EH_INDICES_PER_HASH; ++i) {
        uint idx = g * EH_INDICES_PER_HASH + i;
        if (idx >= EH_INIT_SIZE) break;
        __global uchar *dst = out_hashes + (size_t)idx * 12;
        for (int b = 0; b < 12; ++b) dst[b] = tmp[i * 12 + b];
    }
}
)CLC";

std::string board_hint_from_name(const std::string& name) {
    std::string n = name;
    for (auto& c : n) c = (char)tolower((unsigned char)c);
    if (n.find("6800") != std::string::npos && n.find("xt") != std::string::npos) return "6800xt";
    if (n.find("5700") != std::string::npos && n.find("xt") != std::string::npos) return "5700xt";
    if (n.find("6800 xt") != std::string::npos) return "6800xt";
    if (n.find("5700 xt") != std::string::npos) return "5700xt";
    if (n.find("gfx1030") != std::string::npos || n.find("navi 21") != std::string::npos) return "6800xt";
    if (n.find("gfx1010") != std::string::npos || n.find("navi 10") != std::string::npos) return "5700xt";
    return "amd";
}

int eh_solve_from_hashes(const Blake2bState& base, const uint8_t* hashes,
                         const std::function<void(const EquihashSolution&)>& on_sol,
                         std::atomic<bool>* cancel);

} // namespace

struct OpenClSolver::Impl {
    struct Dev {
        GpuDeviceInfo info;
        cl_device_id id = nullptr;
        cl_context ctx = nullptr;
        cl_command_queue q = nullptr;
        cl_program prog = nullptr;
        cl_kernel kgen = nullptr;
        cl_mem buf_h = nullptr;
        cl_mem buf_tail = nullptr;
        cl_mem buf_out = nullptr;
        std::unique_ptr<std::mutex> mu = std::make_unique<std::mutex>();
    };
    cl_platform_id platform = nullptr;
    std::vector<Dev> devs;

    ~Impl() {
        for (auto& d : devs) {
            if (d.buf_out) clReleaseMemObject(d.buf_out);
            if (d.buf_tail) clReleaseMemObject(d.buf_tail);
            if (d.buf_h) clReleaseMemObject(d.buf_h);
            if (d.kgen) clReleaseKernel(d.kgen);
            if (d.prog) clReleaseProgram(d.prog);
            if (d.q) clReleaseCommandQueue(d.q);
            if (d.ctx) clReleaseContext(d.ctx);
        }
    }
};

bool OpenClSolver::available() {
    cl_uint n = 0;
    return clGetPlatformIDs(0, nullptr, &n) == CL_SUCCESS && n > 0;
}

std::vector<GpuDeviceInfo> OpenClSolver::list_devices() {
    std::vector<GpuDeviceInfo> out;
    cl_uint np = 0;
    if (clGetPlatformIDs(0, nullptr, &np) != CL_SUCCESS || np == 0) return out;
    std::vector<cl_platform_id> plats(np);
    clGetPlatformIDs(np, plats.data(), nullptr);
    int idx = 0;
    for (auto p : plats) {
        cl_uint nd = 0;
        if (clGetDeviceIDs(p, CL_DEVICE_TYPE_GPU, 0, nullptr, &nd) != CL_SUCCESS || nd == 0) continue;
        std::vector<cl_device_id> ids(nd);
        clGetDeviceIDs(p, CL_DEVICE_TYPE_GPU, nd, ids.data(), nullptr);
        for (auto id : ids) {
            GpuDeviceInfo info;
            info.index = idx++;
            char name[256] = {}, vendor[256] = {}, ver[256] = {};
            clGetDeviceInfo(id, CL_DEVICE_NAME, sizeof(name), name, nullptr);
            clGetDeviceInfo(id, CL_DEVICE_VENDOR, sizeof(vendor), vendor, nullptr);
            clGetDeviceInfo(id, CL_DEVICE_VERSION, sizeof(ver), ver, nullptr);
            clGetDeviceInfo(id, CL_DEVICE_GLOBAL_MEM_SIZE, sizeof(info.global_mem), &info.global_mem, nullptr);
            clGetDeviceInfo(id, CL_DEVICE_MAX_COMPUTE_UNITS, sizeof(info.compute_units), &info.compute_units, nullptr);
            info.name = name;
            info.vendor = vendor;
            info.version = ver;
            std::string v = vendor, n = name;
            for (auto& c : v) c = (char)tolower((unsigned char)c);
            for (auto& c : n) c = (char)tolower((unsigned char)c);
            info.amd = v.find("advanced micro") != std::string::npos || v.find("amd") != std::string::npos
                    || n.find("radeon") != std::string::npos || n.find("gfx") != std::string::npos;
            info.board_hint = board_hint_from_name(info.name);
            out.push_back(info);
        }
    }
    return out;
}

OpenClSolver::OpenClSolver() : impl_(std::make_unique<Impl>()) {}
OpenClSolver::~OpenClSolver() = default;

bool OpenClSolver::init(const std::vector<int>& device_indices) {
    auto listed = list_devices();
    if (listed.empty()) {
        LOGE("Keine OpenCL-GPU gefunden");
        return false;
    }

    cl_uint np = 0;
    clGetPlatformIDs(0, nullptr, &np);
    std::vector<cl_platform_id> plats(np);
    clGetPlatformIDs(np, plats.data(), nullptr);

    struct Pair { GpuDeviceInfo info; cl_device_id id; };
    std::vector<Pair> all;
    int idx = 0;
    for (auto p : plats) {
        cl_uint nd = 0;
        if (clGetDeviceIDs(p, CL_DEVICE_TYPE_GPU, 0, nullptr, &nd) != CL_SUCCESS || !nd) continue;
        std::vector<cl_device_id> ids(nd);
        clGetDeviceIDs(p, CL_DEVICE_TYPE_GPU, nd, ids.data(), nullptr);
        for (size_t i = 0; i < ids.size(); ++i) {
            all.push_back({listed[idx], ids[i]});
            idx++;
        }
    }

    std::vector<Pair> chosen;
    if (device_indices.empty()) {
        for (auto& p : all) if (p.info.amd) chosen.push_back(p);
        if (chosen.empty()) chosen = all;
    } else {
        for (int di : device_indices) {
            if (di < 0 || di >= (int)all.size()) {
                LOGW("Ungueltige GPU-ID %d ignoriert", di);
                continue;
            }
            chosen.push_back(all[di]);
        }
    }
    if (chosen.empty()) {
        LOGE("Keine passenden OpenCL-Geraete ausgewaehlt");
        return false;
    }

    for (auto& c : chosen) {
        Impl::Dev d;
        d.info = c.info;
        d.id = c.id;
        cl_int err = 0;
        d.ctx = clCreateContext(nullptr, 1, &d.id, nullptr, nullptr, &err);
        if (err != CL_SUCCESS) {
            LOGE("clCreateContext fehlgeschlagen (%d) fuer %s", err, d.info.name.c_str());
            continue;
        }
#ifdef CL_VERSION_2_0
        d.q = clCreateCommandQueueWithProperties(d.ctx, d.id, nullptr, &err);
        if (err != CL_SUCCESS)
#endif
        {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
            d.q = clCreateCommandQueue(d.ctx, d.id, 0, &err);
#pragma GCC diagnostic pop
        }
        if (err != CL_SUCCESS) {
            LOGE("clCreateCommandQueue fehlgeschlagen (%d)", err);
            continue;
        }
        const char* src = kKernelSource;
        size_t slen = std::strlen(src);
        d.prog = clCreateProgramWithSource(d.ctx, 1, &src, &slen, &err);
        err = clBuildProgram(d.prog, 1, &d.id, "-cl-fast-relaxed-math", nullptr, nullptr);
        if (err != CL_SUCCESS) {
            size_t logn = 0;
            clGetProgramBuildInfo(d.prog, d.id, CL_PROGRAM_BUILD_LOG, 0, nullptr, &logn);
            std::string log(logn, '\0');
            clGetProgramBuildInfo(d.prog, d.id, CL_PROGRAM_BUILD_LOG, logn, log.data(), nullptr);
            LOGE("OpenCL Build-Fehler auf %s:\n%s", d.info.name.c_str(), log.c_str());
            continue;
        }
        d.kgen = clCreateKernel(d.prog, "generate_hashes", &err);
        d.buf_h = clCreateBuffer(d.ctx, CL_MEM_READ_ONLY, 8 * sizeof(cl_ulong), nullptr, &err);
        d.buf_tail = clCreateBuffer(d.ctx, CL_MEM_READ_ONLY, 128, nullptr, &err);
        d.buf_out = clCreateBuffer(d.ctx, CL_MEM_WRITE_ONLY, (size_t)EH_INIT_SIZE * 12, nullptr, &err);
        if (err != CL_SUCCESS) {
            LOGE("OpenCL Buffer fehlgeschlagen (%d)", err);
            continue;
        }
        LOGI("OpenCL GPU %d: %s  (%.1f GiB, %u CUs%s)",
             d.info.index, d.info.name.c_str(),
             d.info.global_mem / 1024.0 / 1024.0 / 1024.0,
             d.info.compute_units,
             d.info.board_hint == "6800xt" ? ", RX 6800 XT Tuning" :
             d.info.board_hint == "5700xt" ? ", RX 5700 XT Tuning" : "");
        impl_->devs.push_back(std::move(d));
        devices_.push_back(c.info);
    }
    ready_ = !impl_->devs.empty();
    return ready_;
}

bool OpenClSolver::generate_hashes(int dev, const uint8_t prefix[180], const uint8_t nonce[32],
                                   std::vector<uint8_t>& hashes_out) {
    if (dev < 0 || dev >= (int)impl_->devs.size()) return false;
    auto& d = impl_->devs[dev];
    std::lock_guard<std::mutex> lock(*d.mu);

    Blake2bState S;
    eh_init_state(&S, prefix, nonce);

    cl_int err;
    err = clEnqueueWriteBuffer(d.q, d.buf_h, CL_TRUE, 0, 8 * sizeof(cl_ulong), S.h, 0, nullptr, nullptr);
    err |= clEnqueueWriteBuffer(d.q, d.buf_tail, CL_TRUE, 0, S.buflen, S.buf, 0, nullptr, nullptr);
    cl_ulong t0 = S.t[0], t1 = S.t[1];
    cl_uint tail_len = (cl_uint)S.buflen;
    clSetKernelArg(d.kgen, 0, sizeof(cl_mem), &d.buf_h);
    clSetKernelArg(d.kgen, 1, sizeof(cl_ulong), &t0);
    clSetKernelArg(d.kgen, 2, sizeof(cl_ulong), &t1);
    clSetKernelArg(d.kgen, 3, sizeof(cl_mem), &d.buf_tail);
    clSetKernelArg(d.kgen, 4, sizeof(cl_uint), &tail_len);
    clSetKernelArg(d.kgen, 5, sizeof(cl_mem), &d.buf_out);

    size_t max_g = (EH_INIT_SIZE + EH_INDICES_PER_HASH - 1) / EH_INDICES_PER_HASH;
    size_t local = 64;
    if (d.info.board_hint == "6800xt") local = 64;
    if (d.info.board_hint == "5700xt") local = 64;
    size_t global = ((max_g + local - 1) / local) * local;
    err = clEnqueueNDRangeKernel(d.q, d.kgen, 1, nullptr, &global, &local, 0, nullptr, nullptr);
    hashes_out.resize((size_t)EH_INIT_SIZE * 12);
    err |= clEnqueueReadBuffer(d.q, d.buf_out, CL_TRUE, 0, hashes_out.size(), hashes_out.data(), 0, nullptr, nullptr);
    if (err != CL_SUCCESS) {
        LOGE("OpenCL generate_hashes Fehler %d auf GPU %d", err, dev);
        return false;
    }
    return true;
}

int OpenClSolver::solve(int dev, const uint8_t prefix[180], const uint8_t nonce[32],
                        const std::function<void(const EquihashSolution&)>& on_sol,
                        std::atomic<bool>* cancel) {
    std::vector<uint8_t> hashes;
    if (!generate_hashes(dev, prefix, nonce, hashes)) {
        return eh_solve_from_header(prefix, nonce, on_sol, cancel);
    }
    Blake2bState base;
    eh_init_state(&base, prefix, nonce);
    return eh_solve_from_hashes(base, hashes.data(), on_sol, cancel);
}

int eh_solve_from_header(const uint8_t prefix[180], const uint8_t nonce[32],
                         const std::function<void(const EquihashSolution&)>& on_sol,
                         std::atomic<bool>* cancel) {
    Blake2bState base;
    eh_init_state(&base, prefix, nonce);
    return eh_solve_cpu(base, on_sol, cancel);
}

namespace {

void ExpandArrayLocal(const unsigned char* in, size_t in_len,
                      unsigned char* out, size_t bit_len) {
    size_t out_width = (bit_len + 7) / 8;
    uint32_t bit_len_mask = ((uint32_t)1 << bit_len) - 1;
    size_t acc_bits = 0;
    uint32_t acc_value = 0;
    size_t j = 0;
    for (size_t i = 0; i < in_len; i++) {
        acc_value = (acc_value << 8) | in[i];
        acc_bits += 8;
        if (acc_bits >= bit_len) {
            acc_bits -= bit_len;
            for (size_t x = 0; x < out_width; x++) {
                out[j + x] = (uint8_t)(
                    (acc_value >> (acc_bits + (8 * (out_width - x - 1)))) &
                    ((bit_len_mask >> (8 * (out_width - x - 1))) & 0xFF));
            }
            j += out_width;
        }
    }
}

int eh_solve_from_hashes(const Blake2bState& base, const uint8_t* hashes,
                         const std::function<void(const EquihashSolution&)>& on_sol,
                         std::atomic<bool>* cancel) {
    struct Row {
        uint8_t hash[12];
        uint8_t idx[128];
    };
    std::vector<Row> X(EH_INIT_SIZE);
    for (int n = 0; n < EH_INIT_SIZE; ++n) {
        ExpandArrayLocal(hashes + n * 12, 12, X[n].hash, 16);
        store32_be(X[n].idx, (uint32_t)n);
    }

    auto has_col = [](const Row& a, const Row& b, int l) {
        return std::memcmp(a.hash, b.hash, l) == 0;
    };
    auto before = [](const Row& a, const Row& b, size_t li) {
        return std::memcmp(a.idx, b.idx, li) < 0;
    };
    auto distinct = [](const Row& a, const Row& b, size_t li) {
        for (size_t i = 0; i < li; i += 4)
            for (size_t j = 0; j < li; j += 4)
                if (std::memcmp(a.idx + i, b.idx + j, 4) == 0) return false;
        return true;
    };
    auto merge = [&](Row& out, const Row& a, const Row& b, size_t hl, size_t li, int trim) {
        for (size_t i = (size_t)trim; i < hl; ++i) out.hash[i - (size_t)trim] = a.hash[i] ^ b.hash[i];
        if (before(a, b, li)) {
            std::memcpy(out.idx, a.idx, li);
            std::memcpy(out.idx + li, b.idx, li);
        } else {
            std::memcpy(out.idx, b.idx, li);
            std::memcpy(out.idx + li, a.idx, li);
        }
    };
    auto zero = [](const uint8_t* h, size_t n) {
        for (size_t i = 0; i < n; ++i) if (h[i]) return false;
        return true;
    };

    size_t hashLen = 12, lenIndices = 4;
    int found = 0;
    for (int r = 1; r < EH_K && !X.empty(); ++r) {
        if (cancel && cancel->load()) return found;
        std::sort(X.begin(), X.end(), [&](const Row& a, const Row& b) {
            return std::memcmp(a.hash, b.hash, 2) < 0;
        });
        std::vector<Row> Xc;
        Xc.reserve(X.size());
        size_t i = 0;
        while (i + 1 < X.size()) {
            size_t j = 1;
            while (i + j < X.size() && has_col(X[i], X[i + j], 2)) j++;
            for (size_t l = 0; l + 1 < j; ++l)
                for (size_t m = l + 1; m < j; ++m)
                    if (distinct(X[i + l], X[i + m], lenIndices)) {
                        Row o{};
                        merge(o, X[i + l], X[i + m], hashLen, lenIndices, 2);
                        Xc.push_back(o);
                    }
            i += j;
        }
        X.swap(Xc);
        hashLen -= 2;
        lenIndices *= 2;
    }
    if (X.size() > 1) {
        std::sort(X.begin(), X.end(), [&](const Row& a, const Row& b) {
            return std::memcmp(a.hash, b.hash, hashLen) < 0;
        });
        size_t i = 0;
        while (i + 1 < X.size()) {
            size_t j = 1;
            while (i + j < X.size() && has_col(X[i], X[i + j], (int)hashLen)) j++;
            for (size_t l = 0; l + 1 < j; ++l)
                for (size_t m = l + 1; m < j; ++m) {
                    if (!distinct(X[i + l], X[i + m], lenIndices)) continue;
                    Row res{};
                    merge(res, X[i + l], X[i + m], hashLen, lenIndices, 0);
                    if (!zero(res.hash, hashLen)) continue;
                    EquihashSolution sol;
                    sol.indices.resize(32);
                    for (int n = 0; n < 32; ++n) sol.indices[n] = load32_be(res.idx + n * 4);
                    sol.compressed = eh_compress_indices(sol.indices.data());
                    if (eh_is_valid_solution(base, sol.compressed.data())) {
                        on_sol(sol);
                        found++;
                    }
                }
            i += j;
        }
    }
    return found;
}

} // namespace
