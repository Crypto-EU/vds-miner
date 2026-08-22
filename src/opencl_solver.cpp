#include "opencl_solver.hpp"
#include "util.hpp"
#include "kernel_embed.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <chrono>
#include <cstring>
#include <cstdio>
#include <fstream>
#include <map>
#include <memory>
#include <mutex>
#include <sstream>
#include <thread>
#include <vector>

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

cl_int wait_cl_event(cl_event ev) {
    if (!ev) return CL_INVALID_EVENT;
    for (;;) {
        cl_int st = 0;
        cl_int e = clGetEventInfo(ev, CL_EVENT_COMMAND_EXECUTION_STATUS, sizeof(st), &st, nullptr);
        if (e != CL_SUCCESS) {
            clReleaseEvent(ev);
            return e;
        }
        if (st == CL_COMPLETE) {
            clReleaseEvent(ev);
            return CL_SUCCESS;
        }
        if (st < 0) {
            clReleaseEvent(ev);
            return st;
        }
        std::this_thread::sleep_for(std::chrono::microseconds(500));
    }
}

cl_int enqueue_write(cl_command_queue q, cl_mem mem, size_t sz, const void* p) {
    if (sz == 0) return CL_SUCCESS;
    return clEnqueueWriteBuffer(q, mem, CL_FALSE, 0, sz, p, 0, nullptr, nullptr);
}

cl_int enqueue_read_wait(cl_command_queue q, cl_mem mem, size_t sz, void* p) {
    cl_event ev = nullptr;
    cl_int e = clEnqueueReadBuffer(q, mem, CL_FALSE, 0, sz, p, 0, nullptr, &ev);
    if (e != CL_SUCCESS) return e;
    clFlush(q);
    return wait_cl_event(ev);
}

cl_command_queue make_queue(cl_context ctx, cl_device_id id, cl_int* err) {
    cl_int e = CL_SUCCESS;
    cl_command_queue q = nullptr;
#ifdef CL_VERSION_2_0
    q = clCreateCommandQueueWithProperties(ctx, id, nullptr, &e);
    if (e == CL_SUCCESS && q) {
        if (err) *err = e;
        return q;
    }
#endif
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    q = clCreateCommandQueue(ctx, id, 0, &e);
#pragma GCC diagnostic pop
    if (err) *err = e;
    return q;
}

} // namespace

struct OpenClSolver::Impl {
    struct Pipe {
        cl_command_queue q = nullptr;
        cl_kernel kgen = nullptr;
        cl_kernel kpack = nullptr;
        cl_kernel kzero = nullptr;
        cl_kernel kfill = nullptr;
        cl_kernel kpairs = nullptr;
        cl_kernel kfinal = nullptr;
        cl_kernel kprep = nullptr;
        cl_mem buf_h = nullptr;
        cl_mem buf_tail = nullptr;
        cl_mem buf_hashes = nullptr;
        cl_mem buf_items[2]{nullptr, nullptr};
        cl_mem buf_count = nullptr;
        cl_mem buf_slots = nullptr;
        cl_mem buf_outcount = nullptr;
        cl_mem buf_nitems = nullptr;
        cl_mem buf_sols = nullptr;
        cl_mem buf_nsols = nullptr;
        std::unique_ptr<std::mutex> mu = std::make_unique<std::mutex>();
    };
    struct Dev {
        GpuDeviceInfo info;
        cl_device_id id = nullptr;
        cl_context ctx = nullptr;
        cl_program prog = nullptr;
        size_t local256 = 256;
        size_t local64 = 64;
        size_t max_wg = 256;
        int pipes_used = 1;
        std::vector<Pipe> pipes;
    };
    std::vector<Dev> devs;

    static void release_pipe(Pipe& p) {
        auto relm = [](cl_mem& m) { if (m) { clReleaseMemObject(m); m = nullptr; } };
        auto relk = [](cl_kernel& k) { if (k) { clReleaseKernel(k); k = nullptr; } };
        relm(p.buf_nsols); relm(p.buf_sols); relm(p.buf_nitems); relm(p.buf_outcount);
        relm(p.buf_slots); relm(p.buf_count);
        relm(p.buf_items[0]); relm(p.buf_items[1]);
        relm(p.buf_hashes); relm(p.buf_tail); relm(p.buf_h);
        relk(p.kprep); relk(p.kfinal); relk(p.kpairs); relk(p.kfill);
        relk(p.kzero); relk(p.kpack); relk(p.kgen);
        if (p.q) { clReleaseCommandQueue(p.q); p.q = nullptr; }
    }

    ~Impl() {
        for (auto& d : devs) {
            for (auto& p : d.pipes) release_pipe(p);
            if (d.prog) clReleaseProgram(d.prog);
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

bool OpenClSolver::init(const std::vector<int>& device_indices, int pipes) {
    pipes = std::max(1, std::min(2, pipes));
    pipes_ = pipes;
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
        size_t max_wg = 256;
        clGetDeviceInfo(d.id, CL_DEVICE_MAX_WORK_GROUP_SIZE, sizeof(max_wg), &max_wg, nullptr);
        if (max_wg == 0) max_wg = 64;
        d.local256 = std::min((size_t)256, max_wg);
        d.local64 = std::min((size_t)64, max_wg);
        if (d.local256 < 32) d.local256 = max_wg;
        if (d.local64 < 8) d.local64 = max_wg;
        d.max_wg = max_wg;
        const char* src = kKernelSource;
        size_t slen = std::strlen(src);
        d.prog = clCreateProgramWithSource(d.ctx, 1, &src, &slen, &err);
        err = clBuildProgram(d.prog, 1, &d.id, "-cl-std=CL1.2 -cl-mad-enable", nullptr, nullptr);
        if (err != CL_SUCCESS) {
            size_t logn = 0;
            clGetProgramBuildInfo(d.prog, d.id, CL_PROGRAM_BUILD_LOG, 0, nullptr, &logn);
            std::string log(logn, '\0');
            clGetProgramBuildInfo(d.prog, d.id, CL_PROGRAM_BUILD_LOG, logn, log.data(), nullptr);
            LOGE("OpenCL Build-Fehler auf %s:\n%s", d.info.name.c_str(), log.c_str());
            continue;
        }

        auto mk_pipe = [&](Impl::Pipe& p) -> bool {
            cl_int e = 0;
            p.q = make_queue(d.ctx, d.id, &e);
            if (e != CL_SUCCESS || !p.q) {
                LOGE("clCreateCommandQueue fehlgeschlagen (%d)", e);
                return false;
            }
            auto mk = [&](const char* name) -> cl_kernel {
                cl_int ke = 0;
                cl_kernel k = clCreateKernel(d.prog, name, &ke);
                if (ke != CL_SUCCESS) LOGE("Kernel %s: %d", name, ke);
                return k;
            };
            p.kgen = mk("generate_hashes");
            p.kpack = mk("pack_items");
            p.kzero = mk("zero_u32");
            p.kfill = mk("fill_buckets");
            p.kpairs = mk("emit_pairs");
            p.kfinal = mk("emit_final");
            p.kprep = mk("prepare_round");
            if (!p.kgen || !p.kpack || !p.kzero || !p.kfill || !p.kpairs || !p.kfinal || !p.kprep) return false;

            p.buf_h = clCreateBuffer(d.ctx, CL_MEM_READ_ONLY, 8 * sizeof(cl_ulong), nullptr, &e);
            p.buf_tail = clCreateBuffer(d.ctx, CL_MEM_READ_ONLY, 128, nullptr, &e);
            p.buf_hashes = clCreateBuffer(d.ctx, CL_MEM_READ_WRITE, (size_t)EH_INIT_SIZE * 12, nullptr, &e);
            p.buf_items[0] = clCreateBuffer(d.ctx, CL_MEM_READ_WRITE, (size_t)kMaxItems * sizeof(GpuItem), nullptr, &e);
            p.buf_items[1] = clCreateBuffer(d.ctx, CL_MEM_READ_WRITE, (size_t)kMaxItems * sizeof(GpuItem), nullptr, &e);
            p.buf_count = clCreateBuffer(d.ctx, CL_MEM_READ_WRITE, (size_t)kNBuckets * sizeof(cl_uint), nullptr, &e);
            p.buf_slots = clCreateBuffer(d.ctx, CL_MEM_READ_WRITE, (size_t)kNBuckets * kSlotMax * sizeof(cl_uint), nullptr, &e);
            p.buf_outcount = clCreateBuffer(d.ctx, CL_MEM_READ_WRITE, sizeof(cl_uint), nullptr, &e);
            p.buf_nitems = clCreateBuffer(d.ctx, CL_MEM_READ_WRITE, sizeof(cl_uint), nullptr, &e);
            p.buf_sols = clCreateBuffer(d.ctx, CL_MEM_WRITE_ONLY, (size_t)kMaxSols * 32 * sizeof(cl_uint), nullptr, &e);
            p.buf_nsols = clCreateBuffer(d.ctx, CL_MEM_READ_WRITE, sizeof(cl_uint), nullptr, &e);
            if (e != CL_SUCCESS) {
                LOGE("OpenCL Buffer fehlgeschlagen (%d)", e);
                return false;
            }
            return true;
        };

        int got = 0;
        for (int i = 0; i < pipes; ++i) {
            Impl::Pipe p;
            if (!mk_pipe(p)) {
                Impl::release_pipe(p);
                break;
            }
            d.pipes.push_back(std::move(p));
            got++;
        }
        if (got == 0) continue;
        d.pipes_used = got;
        if (got < pipes) {
            LOGW("GPU %d: nur %d/%d Pipelines (VRAM) — weiter mit %d",
                 d.info.index, got, pipes, got);
        }

        LOGI("GPU %d: %s  (%.1f GiB, %u CUs%s)  — %d Pipeline(s), Wagner ohne Host-Sync",
             d.info.index, d.info.name.c_str(),
             d.info.global_mem / 1024.0 / 1024.0 / 1024.0,
             d.info.compute_units,
             d.info.board_hint == "6800xt" ? ", RX 6800 XT" :
             d.info.board_hint == "5700xt" ? ", RX 5700 XT" : "",
             (int)d.pipes.size());
        impl_->devs.push_back(std::move(d));
        devices_.push_back(c.info);
    }
    ready_ = !impl_->devs.empty();
    if (ready_) {
        pipes_ = (int)impl_->devs.front().pipes.size();
        for (auto& d : impl_->devs)
            pipes_ = std::min(pipes_, (int)d.pipes.size());
    }
    return ready_;
}

int OpenClSolver::solve(int dev, const uint8_t prefix[180], const uint8_t nonce[32],
                        const std::function<void(const EquihashSolution&)>& on_sol,
                        std::atomic<bool>* cancel, int pipe, bool log_errors) {
    if (dev < 0 || dev >= (int)impl_->devs.size()) return 0;
    auto& d = impl_->devs[dev];
    if (pipe < 0 || pipe >= (int)d.pipes.size()) pipe = 0;
    auto& p = d.pipes[pipe];
    std::lock_guard<std::mutex> lock(*p.mu);
    if (cancel && cancel->load()) return 0;

    Blake2bState S;
    eh_init_state(&S, prefix, nonce);

    cl_int err = CL_SUCCESS;
    cl_uint ninit = EH_INIT_SIZE;
    cl_uint zero = 0;
    err |= enqueue_write(p.q, p.buf_h, 8 * sizeof(cl_ulong), S.h);
    err |= enqueue_write(p.q, p.buf_tail, S.buflen, S.buf);
    err |= enqueue_write(p.q, p.buf_nitems, sizeof(cl_uint), &ninit);
    err |= enqueue_write(p.q, p.buf_outcount, sizeof(cl_uint), &zero);
    err |= enqueue_write(p.q, p.buf_nsols, sizeof(cl_uint), &zero);

    cl_ulong t0 = S.t[0], t1 = S.t[1];
    cl_uint tail_len = (cl_uint)S.buflen;
    clSetKernelArg(p.kgen, 0, sizeof(cl_mem), &p.buf_h);
    clSetKernelArg(p.kgen, 1, sizeof(cl_ulong), &t0);
    clSetKernelArg(p.kgen, 2, sizeof(cl_ulong), &t1);
    clSetKernelArg(p.kgen, 3, sizeof(cl_mem), &p.buf_tail);
    clSetKernelArg(p.kgen, 4, sizeof(cl_uint), &tail_len);
    clSetKernelArg(p.kgen, 5, sizeof(cl_mem), &p.buf_hashes);

    const size_t local64 = d.local64;
    const size_t local256 = d.local256;
    size_t max_g = (EH_INIT_SIZE + EH_INDICES_PER_HASH - 1) / EH_INDICES_PER_HASH;
    size_t ggen = round_up(max_g, local64);
    err |= clEnqueueNDRangeKernel(p.q, p.kgen, 1, nullptr, &ggen, &local64, 0, nullptr, nullptr);

    clSetKernelArg(p.kpack, 0, sizeof(cl_mem), &p.buf_hashes);
    clSetKernelArg(p.kpack, 1, sizeof(cl_mem), &p.buf_items[0]);
    clSetKernelArg(p.kpack, 2, sizeof(cl_uint), &ninit);
    size_t gpack = round_up(EH_INIT_SIZE, local256);
    err |= clEnqueueNDRangeKernel(p.q, p.kpack, 1, nullptr, &gpack, &local256, 0, nullptr, nullptr);

    auto zero_buckets = [&]() {
        clSetKernelArg(p.kzero, 0, sizeof(cl_mem), &p.buf_count);
        cl_uint n = kNBuckets;
        clSetKernelArg(p.kzero, 1, sizeof(cl_uint), &n);
        size_t g = round_up(kNBuckets, local256);
        return clEnqueueNDRangeKernel(p.q, p.kzero, 1, nullptr, &g, &local256, 0, nullptr, nullptr);
    };
    auto nd_fill = [&]() {
        size_t g = round_up(kMaxItems, local256);
        return clEnqueueNDRangeKernel(p.q, p.kfill, 1, nullptr, &g, &local256, 0, nullptr, nullptr);
    };
    auto nd_pairs = [&]() {
        size_t g = round_up(kNBuckets, local256);
        return clEnqueueNDRangeKernel(p.q, p.kpairs, 1, nullptr, &g, &local256, 0, nullptr, nullptr);
    };
    auto nd_prep = [&]() {
        clSetKernelArg(p.kprep, 0, sizeof(cl_mem), &p.buf_outcount);
        clSetKernelArg(p.kprep, 1, sizeof(cl_mem), &p.buf_nitems);
        clSetKernelArg(p.kprep, 2, sizeof(cl_mem), &p.buf_count);
        size_t g = round_up(kNBuckets, local256);
        return clEnqueueNDRangeKernel(p.q, p.kprep, 1, nullptr, &g, &local256, 0, nullptr, nullptr);
    };

    err |= zero_buckets();

    int src = 0;
    uint32_t hash_len = 12;
    uint32_t nidx = 1;

    for (int r = 0; r < EH_K - 1; ++r) {
        if (cancel && cancel->load()) return 0;
        int dst = 1 - src;
        cl_uint trim = 2;
        clSetKernelArg(p.kfill, 0, sizeof(cl_mem), &p.buf_items[src]);
        clSetKernelArg(p.kfill, 1, sizeof(cl_mem), &p.buf_nitems);
        clSetKernelArg(p.kfill, 2, sizeof(cl_mem), &p.buf_count);
        clSetKernelArg(p.kfill, 3, sizeof(cl_mem), &p.buf_slots);
        err |= nd_fill();

        clSetKernelArg(p.kpairs, 0, sizeof(cl_mem), &p.buf_items[src]);
        clSetKernelArg(p.kpairs, 1, sizeof(cl_mem), &p.buf_count);
        clSetKernelArg(p.kpairs, 2, sizeof(cl_mem), &p.buf_slots);
        clSetKernelArg(p.kpairs, 3, sizeof(cl_mem), &p.buf_items[dst]);
        clSetKernelArg(p.kpairs, 4, sizeof(cl_mem), &p.buf_outcount);
        clSetKernelArg(p.kpairs, 5, sizeof(cl_uint), &hash_len);
        clSetKernelArg(p.kpairs, 6, sizeof(cl_uint), &nidx);
        clSetKernelArg(p.kpairs, 7, sizeof(cl_uint), &trim);
        err |= nd_pairs();
        err |= nd_prep();
        src = dst;
        hash_len -= 2;
        nidx *= 2;
    }

    clSetKernelArg(p.kfill, 0, sizeof(cl_mem), &p.buf_items[src]);
    clSetKernelArg(p.kfill, 1, sizeof(cl_mem), &p.buf_nitems);
    clSetKernelArg(p.kfill, 2, sizeof(cl_mem), &p.buf_count);
    clSetKernelArg(p.kfill, 3, sizeof(cl_mem), &p.buf_slots);
    err |= nd_fill();

    clSetKernelArg(p.kfinal, 0, sizeof(cl_mem), &p.buf_items[src]);
    clSetKernelArg(p.kfinal, 1, sizeof(cl_mem), &p.buf_count);
    clSetKernelArg(p.kfinal, 2, sizeof(cl_mem), &p.buf_slots);
    clSetKernelArg(p.kfinal, 3, sizeof(cl_mem), &p.buf_sols);
    clSetKernelArg(p.kfinal, 4, sizeof(cl_mem), &p.buf_nsols);
    clSetKernelArg(p.kfinal, 5, sizeof(cl_uint), &hash_len);
    clSetKernelArg(p.kfinal, 6, sizeof(cl_uint), &nidx);
    size_t gbuck = round_up(kNBuckets, local256);
    err |= clEnqueueNDRangeKernel(p.q, p.kfinal, 1, nullptr, &gbuck, &local256, 0, nullptr, nullptr);

    cl_uint nsols = 0;
    err |= enqueue_read_wait(p.q, p.buf_nsols, sizeof(cl_uint), &nsols);
    if (err != CL_SUCCESS) {
        if (log_errors) LOGE("OpenCL Equihash-Fehler %d auf GPU %d", err, dev);
        return -1;
    }
    if (nsols == 0) return 0;
    if (nsols > kMaxSols) nsols = kMaxSols;

    std::vector<uint32_t> sols((size_t)nsols * 32);
    err = enqueue_read_wait(p.q, p.buf_sols, sols.size() * sizeof(uint32_t), sols.data());
    if (err != CL_SUCCESS) return -1;

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

int OpenClSolver::pipes_for(int dev) const {
    if (dev < 0 || dev >= (int)impl_->devs.size()) return 1;
    return std::max(1, impl_->devs[dev].pipes_used);
}

static std::string tune_key(const GpuDeviceInfo& inf) {
    std::string n = inf.name;
    for (char& c : n) {
        if (c == ' ' || c == '\t' || c == '|') c = '_';
    }
    std::ostringstream os;
    os << inf.board_hint << '|' << n << '|' << inf.compute_units << '|' << inf.global_mem;
    return os.str();
}

struct TuneRow {
    int pipes = 1;
    size_t gen = 64;
    size_t wg = 256;
};

static std::map<std::string, TuneRow> load_tune_cache(const std::string& path) {
    std::map<std::string, TuneRow> m;
    std::ifstream in(path);
    if (!in) return m;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream is(line);
        TuneRow r;
        std::string key;
        if (!(is >> key >> r.pipes >> r.gen >> r.wg)) continue;
        r.pipes = std::max(1, std::min(2, r.pipes));
        if (r.gen == 0 || r.wg == 0) continue;
        m[key] = r;
    }
    return m;
}

static void save_tune_cache(const std::string& path, const std::map<std::string, TuneRow>& m) {
    std::string tmp = path + ".tmp";
    {
        std::ofstream out(tmp, std::ios::trunc);
        if (!out) {
            LOGW("Autotune-Cache nicht schreibbar: %s", path.c_str());
            return;
        }
        out << "# vds-miner autotune 1.1.6\n";
        for (auto& kv : m) {
            out << kv.first << ' ' << kv.second.pipes << ' ' << kv.second.gen << ' ' << kv.second.wg << '\n';
        }
        if (!out) {
            LOGW("Autotune-Cache Schreiben fehlgeschlagen: %s", tmp.c_str());
            return;
        }
    }
    if (std::rename(tmp.c_str(), path.c_str()) != 0) {
        LOGW("Autotune-Cache konnte nicht nach %s verschoben werden", path.c_str());
        std::remove(tmp.c_str());
    }
}

void OpenClSolver::autotune(const std::string& cache_path, bool force, std::atomic<bool>* cancel) {
    if (!ready_ || impl_->devs.empty()) return;
    auto cache = load_tune_cache(cache_path);
    std::map<std::string, TuneRow> measured;

    LOGI("Autotune: misst Pipelines und Workgroups je Kartentyp (einmal, dann Cache %s)",
         cache_path.empty() ? "(kein File)" : cache_path.c_str());

    for (int dev = 0; dev < (int)impl_->devs.size(); ++dev) {
        if (cancel && cancel->load()) return;
        auto& d = impl_->devs[dev];
        std::string key = tune_key(d.info);

        if (!force && cache.count(key)) {
            TuneRow r = cache[key];
            if (r.pipes > (int)d.pipes.size()) r.pipes = (int)d.pipes.size();
            if (r.wg > d.max_wg) r.wg = d.max_wg;
            if (r.gen > d.max_wg) r.gen = d.max_wg;
            d.pipes_used = r.pipes;
            d.local64 = r.gen;
            d.local256 = r.wg;
            LOGI("GPU %d: Autotune-Cache  %s  pipes=%d gen=%zu wg=%zu",
                 d.info.index, d.info.name.c_str(), d.pipes_used, d.local64, d.local256);
            continue;
        }
        if (measured.count(key)) {
            TuneRow r = measured[key];
            if (r.pipes > (int)d.pipes.size()) r.pipes = (int)d.pipes.size();
            d.pipes_used = r.pipes;
            d.local64 = r.gen;
            d.local256 = r.wg;
            LOGI("GPU %d: gleiche Karte wie zuvor — pipes=%d gen=%zu wg=%zu",
                 d.info.index, d.pipes_used, d.local64, d.local256);
            continue;
        }

        struct Cand {
            int pipes;
            size_t gen;
            size_t wg;
            double nonce_s = 0;
            double sol_s = 0;
            bool ok = false;
        };
        std::vector<Cand> cands;
        int max_p = (int)d.pipes.size();
        size_t gens[] = {64, 128, 256};
        size_t wgs[] = {64, 128, 256};
        for (int np = 1; np <= max_p; ++np) {
            for (size_t gen : gens) {
                if (gen > d.max_wg) continue;
                for (size_t wg : wgs) {
                    if (wg > d.max_wg) continue;
                    cands.push_back({np, gen, wg});
                }
            }
        }
        if (cands.empty()) cands.push_back({1, d.local64, d.local256});

        Cand best{};
        best.nonce_s = -1;
        uint8_t prefix[180] = {};
        const int warmup = 1;
        const int rounds = 4;

        LOGI("GPU %d Autotune (%s, %u CUs) — %d Kombinationen",
             d.info.index, d.info.name.c_str(), d.info.compute_units, (int)cands.size());

        for (auto& c : cands) {
            if (cancel && cancel->load()) return;
            d.local64 = c.gen;
            d.local256 = c.wg;
            d.pipes_used = c.pipes;

            auto run_pipe = [&](int pipe, int nround, std::atomic<int>* hashes, std::atomic<int>* sols,
                                std::atomic<bool>* bad) {
                for (int r = 0; r < nround; ++r) {
                    if (cancel && cancel->load()) return;
                    if (bad->load()) return;
                    uint8_t n[32] = {};
                    n[0] = (uint8_t)dev;
                    n[1] = (uint8_t)pipe;
                    n[2] = (uint8_t)c.pipes;
                    n[3] = (uint8_t)(c.wg & 0xff);
                    n[4] = (uint8_t)r;
                    n[5] = (uint8_t)(c.gen & 0xff);
                    int found = 0;
                    auto on = [&](const EquihashSolution&) { found++; };
                    int rc = solve(dev, prefix, n, on, cancel, pipe, false);
                    if (rc < 0) {
                        bad->store(true);
                        return;
                    }
                    hashes->fetch_add(1);
                    sols->fetch_add(found);
                }
            };

            std::atomic<int> hashes{0}, sols{0};
            std::atomic<bool> bad{false};
            // warmup, not timed
            {
                std::vector<std::thread> tw;
                for (int p = 0; p < c.pipes; ++p)
                    tw.emplace_back(run_pipe, p, warmup, &hashes, &sols, &bad);
                for (auto& t : tw) t.join();
            }
            if (bad.load()) {
                LOGI("  skip pipes=%d gen=%zu wg=%zu (OpenCL)", c.pipes, c.gen, c.wg);
                continue;
            }
            hashes = 0;
            sols = 0;
            auto t0 = now_ms();
            {
                std::vector<std::thread> tw;
                for (int p = 0; p < c.pipes; ++p)
                    tw.emplace_back(run_pipe, p, rounds, &hashes, &sols, &bad);
                for (auto& t : tw) t.join();
            }
            auto dt = now_ms() - t0;
            if (bad.load() || dt < 20 || hashes.load() <= 0) {
                LOGI("  skip pipes=%d gen=%zu wg=%zu", c.pipes, c.gen, c.wg);
                continue;
            }
            double sec = dt / 1000.0;
            c.nonce_s = hashes.load() / sec;
            c.sol_s = sols.load() / sec;
            c.ok = true;
            LOGI("  pipes=%d gen=%zu wg=%zu  %.3f MH/s  (%d nonce, %d sol, %.1fs)",
                 c.pipes, c.gen, c.wg, sols_to_mhs(c.sol_s), hashes.load(), sols.load(), sec);
            // Highest sols/s wins. Only if within 0.5% prefer fewer pipes (LA).
            bool better = false;
            if (!best.ok) better = true;
            else if (c.sol_s > best.sol_s * 1.005) better = true;
            else if (best.sol_s > c.sol_s * 1.005) better = false;
            else if (c.pipes != best.pipes) better = c.pipes < best.pipes;
            else better = c.nonce_s > best.nonce_s;
            if (better) best = c;
        }

        if (!best.ok) {
            d.pipes_used = std::max(1, std::min(2, (int)d.pipes.size()));
            d.local64 = std::min((size_t)64, d.max_wg);
            d.local256 = std::min((size_t)256, d.max_wg);
            if (d.local64 < 8) d.local64 = d.max_wg;
            if (d.local256 < 32) d.local256 = d.max_wg;
            LOGW("GPU %d: Autotune ohne Treffer, Standard pipes=%d gen=%zu wg=%zu",
                 d.info.index, d.pipes_used, d.local64, d.local256);
            continue;
        }
        d.pipes_used = best.pipes;
        d.local64 = best.gen;
        d.local256 = best.wg;
        measured[key] = {best.pipes, best.gen, best.wg};
        cache[key] = measured[key];
        LOGI("GPU %d: Autotune-Wahl  pipes=%d gen=%zu wg=%zu  %.3f MH/s",
             d.info.index, best.pipes, best.gen, best.wg, sols_to_mhs(best.sol_s));
    }

    if (!cache_path.empty() && !measured.empty()) {
        auto all = cache;
        for (auto& kv : measured) all[kv.first] = kv.second;
        save_tune_cache(cache_path, all);
        LOGI("Autotune gespeichert: %s", cache_path.c_str());
    }
}
