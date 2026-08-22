#include "opencl_solver.hpp"
#include "util.hpp"
#include "kernel_embed.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <memory>
#include <mutex>

#ifdef __APPLE__
#include <OpenCL/opencl.h>
#else
#include <CL/cl.h>
#endif

namespace {

constexpr uint32_t kNBuckets = 65536;
constexpr uint32_t kSlotMax = 12;
constexpr uint32_t kMaxItems = 262144;
constexpr uint32_t kMaxSols = 16;

struct GpuItem {
    uint8_t hash[16];
    uint32_t nidx;
    uint32_t pad[3];
    uint32_t idx[32];
};
static_assert(sizeof(GpuItem) == 160, "GpuItem must match OpenCL Item (160 bytes)");

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

size_t round_up(size_t n, size_t local) {
    return ((n + local - 1) / local) * local;
}

} // namespace

struct OpenClSolver::Impl {
    struct Dev {
        GpuDeviceInfo info;
        cl_device_id id = nullptr;
        cl_context ctx = nullptr;
        cl_command_queue q = nullptr;
        cl_program prog = nullptr;
        cl_kernel kgen = nullptr;
        cl_kernel kpack = nullptr;
        cl_kernel kzero = nullptr;
        cl_kernel kfill = nullptr;
        cl_kernel kpairs = nullptr;
        cl_kernel kfinal = nullptr;
        cl_mem buf_h = nullptr;
        cl_mem buf_tail = nullptr;
        cl_mem buf_hashes = nullptr;
        cl_mem buf_items[2]{nullptr, nullptr};
        cl_mem buf_count = nullptr;
        cl_mem buf_slots = nullptr;
        cl_mem buf_outcount = nullptr;
        cl_mem buf_sols = nullptr;
        cl_mem buf_nsols = nullptr;
        std::unique_ptr<std::mutex> mu = std::make_unique<std::mutex>();
    };
    std::vector<Dev> devs;

    ~Impl() {
        for (auto& d : devs) {
            auto relm = [](cl_mem& m) { if (m) { clReleaseMemObject(m); m = nullptr; } };
            auto relk = [](cl_kernel& k) { if (k) { clReleaseKernel(k); k = nullptr; } };
            relm(d.buf_nsols); relm(d.buf_sols); relm(d.buf_outcount);
            relm(d.buf_slots); relm(d.buf_count);
            relm(d.buf_items[0]); relm(d.buf_items[1]);
            relm(d.buf_hashes); relm(d.buf_tail); relm(d.buf_h);
            relk(d.kfinal); relk(d.kpairs); relk(d.kfill);
            relk(d.kzero); relk(d.kpack); relk(d.kgen);
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
        err = clBuildProgram(d.prog, 1, &d.id, "-cl-std=CL1.2", nullptr, nullptr);
        if (err != CL_SUCCESS) {
            size_t logn = 0;
            clGetProgramBuildInfo(d.prog, d.id, CL_PROGRAM_BUILD_LOG, 0, nullptr, &logn);
            std::string log(logn, '\0');
            clGetProgramBuildInfo(d.prog, d.id, CL_PROGRAM_BUILD_LOG, logn, log.data(), nullptr);
            LOGE("OpenCL Build-Fehler auf %s:\n%s", d.info.name.c_str(), log.c_str());
            continue;
        }

        auto mk = [&](const char* name) -> cl_kernel {
            cl_int e = 0;
            cl_kernel k = clCreateKernel(d.prog, name, &e);
            if (e != CL_SUCCESS) LOGE("Kernel %s: %d", name, e);
            return k;
        };
        d.kgen = mk("generate_hashes");
        d.kpack = mk("pack_items");
        d.kzero = mk("zero_u32");
        d.kfill = mk("fill_buckets");
        d.kpairs = mk("emit_pairs");
        d.kfinal = mk("emit_final");
        if (!d.kgen || !d.kpack || !d.kzero || !d.kfill || !d.kpairs || !d.kfinal) continue;

        d.buf_h = clCreateBuffer(d.ctx, CL_MEM_READ_ONLY, 8 * sizeof(cl_ulong), nullptr, &err);
        d.buf_tail = clCreateBuffer(d.ctx, CL_MEM_READ_ONLY, 128, nullptr, &err);
        d.buf_hashes = clCreateBuffer(d.ctx, CL_MEM_READ_WRITE, (size_t)EH_INIT_SIZE * 12, nullptr, &err);
        d.buf_items[0] = clCreateBuffer(d.ctx, CL_MEM_READ_WRITE, (size_t)kMaxItems * sizeof(GpuItem), nullptr, &err);
        d.buf_items[1] = clCreateBuffer(d.ctx, CL_MEM_READ_WRITE, (size_t)kMaxItems * sizeof(GpuItem), nullptr, &err);
        d.buf_count = clCreateBuffer(d.ctx, CL_MEM_READ_WRITE, (size_t)kNBuckets * sizeof(cl_uint), nullptr, &err);
        d.buf_slots = clCreateBuffer(d.ctx, CL_MEM_READ_WRITE, (size_t)kNBuckets * kSlotMax * sizeof(cl_uint), nullptr, &err);
        d.buf_outcount = clCreateBuffer(d.ctx, CL_MEM_READ_WRITE, sizeof(cl_uint), nullptr, &err);
        d.buf_sols = clCreateBuffer(d.ctx, CL_MEM_WRITE_ONLY, (size_t)kMaxSols * 32 * sizeof(cl_uint), nullptr, &err);
        d.buf_nsols = clCreateBuffer(d.ctx, CL_MEM_READ_WRITE, sizeof(cl_uint), nullptr, &err);
        if (err != CL_SUCCESS) {
            LOGE("OpenCL Buffer fehlgeschlagen (%d)", err);
            continue;
        }
        LOGI("GPU %d: %s  (%.1f GiB, %u CUs%s)  — Equihash laeuft komplett auf der GPU",
             d.info.index, d.info.name.c_str(),
             d.info.global_mem / 1024.0 / 1024.0 / 1024.0,
             d.info.compute_units,
             d.info.board_hint == "6800xt" ? ", RX 6800 XT" :
             d.info.board_hint == "5700xt" ? ", RX 5700 XT" : "");
        impl_->devs.push_back(std::move(d));
        devices_.push_back(c.info);
    }
    ready_ = !impl_->devs.empty();
    return ready_;
}

int OpenClSolver::solve(int dev, const uint8_t prefix[180], const uint8_t nonce[32],
                        const std::function<void(const EquihashSolution&)>& on_sol,
                        std::atomic<bool>* cancel) {
    if (dev < 0 || dev >= (int)impl_->devs.size()) return 0;
    auto& d = impl_->devs[dev];
    std::lock_guard<std::mutex> lock(*d.mu);
    if (cancel && cancel->load()) return 0;

    Blake2bState S;
    eh_init_state(&S, prefix, nonce);

    cl_int err = CL_SUCCESS;
    err |= clEnqueueWriteBuffer(d.q, d.buf_h, CL_TRUE, 0, 8 * sizeof(cl_ulong), S.h, 0, nullptr, nullptr);
    err |= clEnqueueWriteBuffer(d.q, d.buf_tail, CL_TRUE, 0, S.buflen, S.buf, 0, nullptr, nullptr);

    cl_ulong t0 = S.t[0], t1 = S.t[1];
    cl_uint tail_len = (cl_uint)S.buflen;
    clSetKernelArg(d.kgen, 0, sizeof(cl_mem), &d.buf_h);
    clSetKernelArg(d.kgen, 1, sizeof(cl_ulong), &t0);
    clSetKernelArg(d.kgen, 2, sizeof(cl_ulong), &t1);
    clSetKernelArg(d.kgen, 3, sizeof(cl_mem), &d.buf_tail);
    clSetKernelArg(d.kgen, 4, sizeof(cl_uint), &tail_len);
    clSetKernelArg(d.kgen, 5, sizeof(cl_mem), &d.buf_hashes);

    size_t local = 64;
    size_t max_g = (EH_INIT_SIZE + EH_INDICES_PER_HASH - 1) / EH_INDICES_PER_HASH;
    size_t ggen = round_up(max_g, local);
    err |= clEnqueueNDRangeKernel(d.q, d.kgen, 1, nullptr, &ggen, &local, 0, nullptr, nullptr);

    cl_uint ninit = EH_INIT_SIZE;
    clSetKernelArg(d.kpack, 0, sizeof(cl_mem), &d.buf_hashes);
    clSetKernelArg(d.kpack, 1, sizeof(cl_mem), &d.buf_items[0]);
    clSetKernelArg(d.kpack, 2, sizeof(cl_uint), &ninit);
    size_t gpack = round_up(EH_INIT_SIZE, local);
    err |= clEnqueueNDRangeKernel(d.q, d.kpack, 1, nullptr, &gpack, &local, 0, nullptr, nullptr);

    auto zero_buf = [&](cl_mem buf, uint32_t n) {
        clSetKernelArg(d.kzero, 0, sizeof(cl_mem), &buf);
        clSetKernelArg(d.kzero, 1, sizeof(cl_uint), &n);
        size_t g = round_up(n, local);
        return clEnqueueNDRangeKernel(d.q, d.kzero, 1, nullptr, &g, &local, 0, nullptr, nullptr);
    };

    cl_uint nitems = EH_INIT_SIZE;
    int src = 0;
    uint32_t hash_len = 12;
    uint32_t nidx = 1;

    // K-1 = 4 collision rounds, then final round on remaining 4 bytes.
    for (int r = 0; r < EH_K - 1; ++r) {
        if (cancel && cancel->load()) return 0;
        err |= zero_buf(d.buf_count, kNBuckets);
        cl_uint zero = 0;
        err |= clEnqueueWriteBuffer(d.q, d.buf_outcount, CL_TRUE, 0, sizeof(cl_uint), &zero, 0, nullptr, nullptr);

        clSetKernelArg(d.kfill, 0, sizeof(cl_mem), &d.buf_items[src]);
        clSetKernelArg(d.kfill, 1, sizeof(cl_uint), &nitems);
        clSetKernelArg(d.kfill, 2, sizeof(cl_mem), &d.buf_count);
        clSetKernelArg(d.kfill, 3, sizeof(cl_mem), &d.buf_slots);
        size_t gfill = round_up(nitems, local);
        err |= clEnqueueNDRangeKernel(d.q, d.kfill, 1, nullptr, &gfill, &local, 0, nullptr, nullptr);

        int dst = 1 - src;
        cl_uint trim = 2;
        clSetKernelArg(d.kpairs, 0, sizeof(cl_mem), &d.buf_items[src]);
        clSetKernelArg(d.kpairs, 1, sizeof(cl_mem), &d.buf_count);
        clSetKernelArg(d.kpairs, 2, sizeof(cl_mem), &d.buf_slots);
        clSetKernelArg(d.kpairs, 3, sizeof(cl_mem), &d.buf_items[dst]);
        clSetKernelArg(d.kpairs, 4, sizeof(cl_mem), &d.buf_outcount);
        clSetKernelArg(d.kpairs, 5, sizeof(cl_uint), &hash_len);
        clSetKernelArg(d.kpairs, 6, sizeof(cl_uint), &nidx);
        clSetKernelArg(d.kpairs, 7, sizeof(cl_uint), &trim);
        size_t gbuck = round_up(kNBuckets, local);
        err |= clEnqueueNDRangeKernel(d.q, d.kpairs, 1, nullptr, &gbuck, &local, 0, nullptr, nullptr);

        err |= clEnqueueReadBuffer(d.q, d.buf_outcount, CL_TRUE, 0, sizeof(cl_uint), &nitems, 0, nullptr, nullptr);
        if (nitems > kMaxItems) nitems = kMaxItems;
        if (nitems < 2) return 0;
        src = dst;
        hash_len -= 2;
        nidx *= 2;
    }

    err |= zero_buf(d.buf_count, kNBuckets);
    cl_uint nsols = 0;
    err |= clEnqueueWriteBuffer(d.q, d.buf_nsols, CL_TRUE, 0, sizeof(cl_uint), &nsols, 0, nullptr, nullptr);

    clSetKernelArg(d.kfill, 0, sizeof(cl_mem), &d.buf_items[src]);
    clSetKernelArg(d.kfill, 1, sizeof(cl_uint), &nitems);
    clSetKernelArg(d.kfill, 2, sizeof(cl_mem), &d.buf_count);
    clSetKernelArg(d.kfill, 3, sizeof(cl_mem), &d.buf_slots);
    size_t gfill = round_up(nitems, local);
    err |= clEnqueueNDRangeKernel(d.q, d.kfill, 1, nullptr, &gfill, &local, 0, nullptr, nullptr);

    clSetKernelArg(d.kfinal, 0, sizeof(cl_mem), &d.buf_items[src]);
    clSetKernelArg(d.kfinal, 1, sizeof(cl_mem), &d.buf_count);
    clSetKernelArg(d.kfinal, 2, sizeof(cl_mem), &d.buf_slots);
    clSetKernelArg(d.kfinal, 3, sizeof(cl_mem), &d.buf_sols);
    clSetKernelArg(d.kfinal, 4, sizeof(cl_mem), &d.buf_nsols);
    clSetKernelArg(d.kfinal, 5, sizeof(cl_uint), &hash_len);
    clSetKernelArg(d.kfinal, 6, sizeof(cl_uint), &nidx);
    size_t gbuck = round_up(kNBuckets, local);
    err |= clEnqueueNDRangeKernel(d.q, d.kfinal, 1, nullptr, &gbuck, &local, 0, nullptr, nullptr);

    err |= clEnqueueReadBuffer(d.q, d.buf_nsols, CL_TRUE, 0, sizeof(cl_uint), &nsols, 0, nullptr, nullptr);
    if (err != CL_SUCCESS) {
        LOGE("OpenCL Equihash-Fehler %d auf GPU %d", err, dev);
        return 0;
    }
    if (nsols == 0) return 0;
    if (nsols > kMaxSols) nsols = kMaxSols;

    std::vector<uint32_t> sols((size_t)nsols * 32);
    err = clEnqueueReadBuffer(d.q, d.buf_sols, CL_TRUE, 0, sols.size() * sizeof(uint32_t), sols.data(), 0, nullptr, nullptr);
    if (err != CL_SUCCESS) return 0;

    int found = 0;
    for (uint32_t s = 0; s < nsols; ++s) {
        EquihashSolution sol;
        sol.indices.assign(sols.begin() + s * 32, sols.begin() + s * 32 + 32);
        sol.compressed = eh_compress_indices(sol.indices.data());
        if (!eh_is_valid_solution(S, sol.compressed.data())) continue;
        on_sol(sol);
        found++;
    }
    return found;
}
