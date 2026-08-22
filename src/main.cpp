#include "util.hpp"
#include "crypto.hpp"
#include "equihash.hpp"
#include "opencl_solver.hpp"
#include "stratum.hpp"
#include "api.hpp"
#include "stats.hpp"

#include <algorithm>
#include <csignal>
#include <cstring>
#include <fstream>
#include <iostream>
#include <random>
#include <sstream>
#include <thread>
#include <vector>

static std::atomic<bool> g_stop{false};
static void on_sig(int) { g_stop = true; }

static void usage() {
    std::cout <<
R"(vds-miner 1.1.1  —  GPU-Miner fuer VDS (Vollar), Equihash(96,5)+Scrypt

Nutzung:
  vds-miner -o stratum+tcp://HOST:PORT -u WALLET.WORKER [optionen]
  vds-miner --list-devices
  vds-miner --benchmark
  vds-miner --self-test

Pool (666pool):
  -o, --url        stratum+tcp://vds.666pool.com:9338
  -u, --user       VDS-Adresse.Workername
  -p, --pass       Passwort (Standard: x)

Geraete (nur GPU):
  -d, --devices    OpenCL-GPU-IDs, z.B. 0 oder 0,1,2  (Standard: alle AMD-GPUs)

Sonstiges:
  --api-port       HTTP-JSON API (Standard: 4068, HiveOS)
  --worker         Workername, wird an die Wallet gehaengt falls -u keine hat
  -l, --log        0=trace .. 4=error (Standard: 2)
  -h, --help

Beispiele:
  vds-miner -o stratum+tcp://vds.666pool.com:9338 -u VcYourAddress.rig1
  vds-miner -o stratum+tcp://vds.666pool.com:9338 -u VcYourAddress -d 0,1 --intensity 4
)";
}

static std::vector<int> parse_devices(const std::string& s) {
    std::vector<int> d;
    std::stringstream ss(s);
    std::string tok;
    while (std::getline(ss, tok, ',')) {
        if (!tok.empty()) d.push_back(std::stoi(tok));
    }
    return d;
}

static void parse_url(const std::string& url, std::string& host, uint16_t& port) {
    std::string u = url;
    auto scheme = u.find("://");
    if (scheme != std::string::npos) u = u.substr(scheme + 3);
    auto c = u.rfind(':');
    if (c == std::string::npos) {
        host = u;
        port = 9338;
        return;
    }
    host = u.substr(0, c);
    port = (uint16_t)std::stoi(u.substr(c + 1));
}

static bool self_test() {
    LOGI("Self-test: SHA-256");
    uint8_t sh[32];
    sha256((const uint8_t*)"abc", 3, sh);
    if (to_hex(sh, 32) != "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad") {
        LOGE("SHA-256 Fehlschlag: %s", to_hex(sh, 32).c_str());
        return false;
    }

    LOGI("Self-test: BLAKE2b Equihash-Init + Hash");
    uint8_t prefix[180] = {};
    uint8_t nonce[32] = {};
    nonce[31] = 1;
    Blake2bState S;
    eh_init_state(&S, prefix, nonce);
    uint8_t out[60];
    eh_generate_hash(S, 0, out);
    LOGI("  GenerateHash(0) = %s...", to_hex(out, 16).c_str());

    LOGI("Self-test: Equihash(96,5) Referenzsolver (nur Pruefung der Loesung, Mining laeuft auf GPU)");
    std::vector<EquihashSolution> sols;
    std::atomic<bool> cancel{false};
    int n = eh_solve_cpu(S, [&](const EquihashSolution& sol) { sols.push_back(sol); }, &cancel);
    LOGI("  %d Loesung(en) gefunden", n);
    for (auto& sol : sols) {
        if (!eh_is_valid_solution(S, sol.compressed.data())) {
            LOGE("Loesung nicht valide");
            return false;
        }
        uint8_t target[32];
        std::memset(target, 0xff, 32);
        uint8_t powh[32];
        if (!vds_check_pow(prefix, nonce, sol.compressed.data(), target, powh)) {
            LOGE("Scrypt-PoW gegen 0xfff... fehlgeschlagen");
            return false;
        }
        LOGI("  PoW-Hash %s  sol=%s", uint256_to_hex_be(powh).c_str(),
             to_hex(sol.compressed.data(), 8).c_str());
    }
    LOGI("Self-test OK");
    return true;
}

static int run_benchmark(OpenClSolver& gpu) {
    uint8_t prefix[180] = {};
    const int rounds = 8;
    auto t0 = now_ms();
    std::atomic<int> sols{0};
    std::atomic<int> hashes{0};
    std::atomic<bool> cancel{false};

    auto worker = [&](int id) {
        for (int r = 0; r < rounds; ++r) {
            uint8_t n[32] = {};
            n[24] = (uint8_t)id;
            n[28] = (uint8_t)r;
            auto on = [&](const EquihashSolution&) { sols++; };
            gpu.solve(id % gpu.device_count(), prefix, n, on, &cancel);
            hashes++;
        }
    };

    int nw = gpu.device_count();
    std::vector<std::thread> th;
    for (int i = 0; i < nw; ++i) th.emplace_back(worker, i);
    for (auto& t : th) t.join();
    auto dt = now_ms() - t0;
    if (dt == 0) dt = 1;
    LOGI("GPU-Benchmark: %d Iterationen, %d Loesungen in %llu ms",
         hashes.load(), sols.load(), (unsigned long long)dt);
    LOGI("  %.2f I/s   %.2f Sol/s",
         hashes.load() * 1000.0 / dt, sols.load() * 1000.0 / dt);
    return 0;
}

struct WorkCtx {
    StratumClient* stratum = nullptr;
    OpenClSolver* gpu = nullptr;
    MinerStats* stats = nullptr;
    int gpu_index = 0;      // OpenCL solver device slot
    int worker_id = 0;
    int nonce_stride = 1;
    int nonce_offset = 0;
    std::atomic<bool>* stop = nullptr;
};

static void increment_nonce(uint8_t nonce[32], size_t from_byte) {
    for (int i = 31; i >= (int)from_byte; --i) {
        if (++nonce[i] != 0) break;
    }
}

static void miner_thread(WorkCtx ctx) {
    uint64_t local_h = 0, local_s = 0, last_report = now_ms();
    while (ctx.stop && !ctx.stop->load() && !g_stop.load()) {
        StratumJob job;
        if (!ctx.stratum->current_job(job) || !ctx.stratum->authorized()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            continue;
        }
        uint64_t epoch = job.job_epoch;
        uint8_t nonce[32];
        std::memcpy(nonce, job.nonce1, 32);
        // Thread-spezifischer Nonce-Start im freien Bereich nach nonce1
        nonce[19] = (uint8_t)ctx.worker_id; // nheqminer uses byte 19 for thread id
        nonce[18] = (uint8_t)(ctx.worker_id >> 8);

        while (!g_stop.load() && ctx.stratum->job_epoch() == epoch) {
            auto on_sol = [&](const EquihashSolution& sol) {
                local_s++;
                ctx.stats->solutions++;
                uint8_t powh[32];
                if (!vds_check_pow(job.header_prefix, nonce, sol.compressed.data(), job.target, powh))
                    return;
                ctx.stats->shares_found++;
                LOGI("GPU/Worker %d  Share  pow=%s", ctx.worker_id, uint256_to_hex_be(powh).c_str());
                ctx.stratum->submit(job, nonce, sol.compressed.data());
            };
            std::atomic<bool> cancel{false};
            ctx.gpu->solve(ctx.gpu_index, job.header_prefix, nonce, on_sol, &cancel);
            local_h++;
            ctx.stats->hashes++;
            increment_nonce(nonce, job.nonce1_bytes == 0 ? 20 : job.nonce1_bytes);

            auto now = now_ms();
            if (now - last_report >= 10000) {
                double dt = (now - last_report) / 1000.0;
                {
                    std::lock_guard<std::mutex> g(ctx.stats->mu);
                    if ((size_t)ctx.gpu_index < ctx.stats->gpus.size()) {
                        ctx.stats->gpus[ctx.gpu_index].sols_per_s = local_s / dt;
                    }
                }
                LOGI("Worker %d  %.1f I/s  %.1f Sol/s  shares A/R %llu/%llu",
                     ctx.worker_id, local_h / dt, local_s / dt,
                     (unsigned long long)ctx.stratum->accepted(),
                     (unsigned long long)ctx.stratum->rejected());
                ctx.stats->accepted.store(ctx.stratum->accepted());
                ctx.stats->rejected.store(ctx.stratum->rejected());
                local_h = local_s = 0;
                last_report = now;
            }
        }
    }
}

int main(int argc, char** argv) {
    std::string url = "stratum+tcp://vds.666pool.com:9338";
    std::string user, pass = "x", worker, devices_s;
    int log_level = 2;
    int intensity = -1;
    uint16_t api_port = 4068;
    bool list_dev = false, bench = false, test = false;
    std::vector<int> devices;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto need = [&](const char* n) -> std::string {
            if (i + 1 >= argc) { LOGE("Fehlendes Argument fuer %s", n); std::exit(1); }
            return argv[++i];
        };
        if (a == "-h" || a == "--help") { usage(); return 0; }
        else if (a == "-o" || a == "--url") url = need("-o");
        else if (a == "-u" || a == "--user") user = need("-u");
        else if (a == "-p" || a == "--pass") pass = need("-p");
        else if (a == "-d" || a == "--devices") devices_s = need("-d");
        else if (a == "-l" || a == "--log") log_level = std::stoi(need("-l"));
        else if (a == "--api-port") api_port = (uint16_t)std::stoi(need("--api-port"));
        else if (a == "--worker") worker = need("--worker");
        else if (a == "--intensity") intensity = std::stoi(need("--intensity"));
        else if (a == "--list-devices") list_dev = true;
        else if (a == "--benchmark" || a == "-b") bench = true;
        else if (a == "--self-test") test = true;
        else if (a == "-t" || a == "--threads" || a == "--cpu-only") {
            LOGE("CPU-Mining ist entfernt. Der Miner laeuft nur auf der GPU.");
            return 1;
        }
        else { LOGE("Unbekannte Option: %s", a.c_str()); usage(); return 1; }
    }
    Log::instance().set_level((Log::Level)std::max(0, std::min(4, log_level)));
    (void)intensity;

    if (test) return self_test() ? 0 : 1;

    if (list_dev) {
        auto devs = OpenClSolver::list_devices();
        if (devs.empty()) {
            LOGE("Keine OpenCL-GPUs gefunden. AMD-Treiber mit OpenCL installieren.");
            return 1;
        }
        for (auto& d : devs) {
            std::printf("[%d] %s  vendor=%s  %.1f GiB  %u CUs  amd=%s  hint=%s\n",
                        d.index, d.name.c_str(), d.vendor.c_str(),
                        d.global_mem / 1024.0 / 1024.0 / 1024.0, d.compute_units,
                        d.amd ? "yes" : "no", d.board_hint.c_str());
        }
        return 0;
    }

    OpenClSolver gpu;
    if (!devices_s.empty()) devices = parse_devices(devices_s);
    if (!gpu.init(devices)) {
        LOGE("Keine nutzbare OpenCL-GPU. vds-miner laeuft nur auf GPU (RX 5700 XT / 6800 XT / HiveOS).");
        LOGE("Bitte amdgpu-pro oder ROCm OpenCL installieren und --list-devices pruefen.");
        return 1;
    }

    if (bench) return run_benchmark(gpu);

    if (user.empty()) {
        LOGE("Bitte Wallet angeben: -u VDSADRESSE.worker");
        usage();
        return 1;
    }
    if (!worker.empty() && user.find('.') == std::string::npos) user += "." + worker;

    {
        auto dot = user.find('.');
        std::string wallet = dot == std::string::npos ? user : user.substr(0, dot);
        if (wallet.size() >= 2 && wallet[0] == '0' && (wallet[1] == 'x' || wallet[1] == 'X')) {
            LOGW("Wallet '%s' sieht nach einer 0x/ETH-Adresse aus.", wallet.c_str());
            LOGW("666pool VDS erwartet eine VDS-Adresse, typischerweise beginnend mit V.");
        }
    }

    std::string host;
    uint16_t port = 9338;
    parse_url(url, host, port);

    std::signal(SIGINT, on_sig);
    std::signal(SIGTERM, on_sig);

    MinerStats stats;
    stats.start_ms = now_ms();
    {
        std::lock_guard<std::mutex> g(stats.mu);
        for (auto& d : gpu.devices()) {
            GpuStats gs;
            gs.name = d.name;
            stats.gpus.push_back(gs);
        }
    }

    StratumClient stratum;
    stratum.set_endpoint(host, port, user, pass);
    stratum.start();

    ApiServer api(stats, api_port);
    api.start();

    LOGI("vds-miner 1.1.1  |  VDS Equihash(96,5)+Scrypt GPU-only  |  Pool %s:%u  |  User %s",
         host.c_str(), port, user.c_str());

    int nworkers = 0;
    std::vector<std::thread> workers;
    for (int g = 0; g < gpu.device_count(); ++g) {
        int streams = 1;
        if (intensity > 0) streams = std::max(1, std::min(8, intensity));
        // Ein Command-Queue pro GPU; zusaetzliche Streams teilen sich die Queue.
        // 1 Stream ist der Normalfall — Equihash laeuft vollstaendig auf der GPU.
        (void)streams;
        WorkCtx ctx;
        ctx.stratum = &stratum;
        ctx.gpu = &gpu;
        ctx.stats = &stats;
        ctx.gpu_index = g;
        ctx.worker_id = nworkers++;
        ctx.stop = &g_stop;
        workers.emplace_back(miner_thread, ctx);
        LOGI("GPU %d (%s): Equihash+Scrypt-Mining auf der GPU", g, gpu.devices()[g].name.c_str());
    }
    LOGI("%d GPU-Worker gestartet", nworkers);

    while (!g_stop.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        stats.accepted.store(stratum.accepted());
        stats.rejected.store(stratum.rejected());
    }

    LOGI("Fahre herunter...");
    stratum.stop();
    for (auto& t : workers) if (t.joinable()) t.join();
    api.stop();
    return 0;
}
