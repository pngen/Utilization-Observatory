// Utilization Observatory : benchmarks (completed operations, not timeouts).
// Copyright 2026 Summon Software Labs. Apache-2.0.
#include "uo/observatory.hpp"
#include "uo/source.hpp"
#include "uo/time.hpp"
#include "uo/view.hpp"
#include <chrono>
#include <cstdio>
#include <string>
#include <vector>

using namespace uo;

static const char* scale_name(size_t n) {
    if (n >= 1000000) return "1M"; if (n >= 100000) return "100k"; if (n >= 10000) return "10k"; if (n >= 1000) return "1k"; return "100";
}

static double ms_since(std::chrono::steady_clock::time_point a) {
    return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - a).count() / 1000.0;
}

static void bench_ingest(size_t n) {
    UtilizationObservatory obs;
    auto t0 = std::chrono::steady_clock::now();
    for (size_t i = 0; i < n; ++i) {
        for (int k = 0; k < 2; ++k) {
            Observation o;
            o.id = ObservationId(i * 2 + k);
            o.coordinator_epoch = CoordinatorEpoch(1);
            o.type = k == 0 ? ObservationType::ExecutionBegin : ObservationType::ExecutionEnd;
            o.begin = static_cast<Tick>(i * 100);
            o.end = o.begin + (k == 0 ? 0 : 40);
            o.source = SourceId(1); o.source_generation = SourceGeneration(1);
            o.worker = WorkerId(1); o.worker_boot = WorkerBootId(1);
            o.device = DeviceId(1); o.device_generation = DeviceGeneration(1);
            o.state = StateCategory::UsefulExecution;
            obs.ingest(o);
        }
    }
    double ms = ms_since(t0);
    size_t ops = n * 2;
    std::printf("  ingest %-6s: %8zu obs in %8.2f ms  (%llu ops/s)\n", scale_name(ops), ops, ms,
                (unsigned long long)(ops / (ms / 1000.0)));
}

static void bench_query(size_t n) {
    UtilizationObservatory obs;
    for (size_t i = 0; i < n; ++i) {
        Observation b;
        b.id = ObservationId(i * 2); b.coordinator_epoch = CoordinatorEpoch(1);
        b.type = ObservationType::ExecutionBegin; b.begin = static_cast<Tick>(i * 100); b.end = b.begin;
        b.source = SourceId(1); b.source_generation = SourceGeneration(1); b.worker = WorkerId(1); b.worker_boot = WorkerBootId(1);
        b.device = DeviceId(1); b.device_generation = DeviceGeneration(1); b.state = StateCategory::UsefulExecution;
        obs.ingest(b);
        Observation e;
        e.id = ObservationId(i * 2 + 1); e.coordinator_epoch = CoordinatorEpoch(1);
        e.type = ObservationType::ExecutionEnd; e.begin = static_cast<Tick>(i * 100 + 40); e.end = e.begin;
        e.source = SourceId(1); e.source_generation = SourceGeneration(1); e.worker = WorkerId(1); e.worker_boot = WorkerBootId(1);
        e.device = DeviceId(1); e.device_generation = DeviceGeneration(1); e.state = StateCategory::UsefulExecution;
        obs.ingest(e);
    }
    auto t0 = std::chrono::steady_clock::now();
    for (int rep = 0; rep < 100; ++rep) {
        auto v = obs.device_window(DeviceId(1), Interval{0, static_cast<Tick>(n * 100)});
        (void)v;
        auto g = obs.gap(DeviceId(1), Interval{0, static_cast<Tick>(n * 100)});
        (void)g;
    }
    double ms = ms_since(t0);
    std::printf("  window+gap %-5s: %llu reps in %8.2f ms\n", scale_name(n), (unsigned long long)100, ms);
}

static void bench_persistence(size_t n) {
    UtilizationObservatory obs;
    for (size_t i = 0; i < n; ++i) {
        Observation b;
        b.id = ObservationId(i * 2); b.coordinator_epoch = CoordinatorEpoch(1);
        b.type = ObservationType::ExecutionBegin; b.begin = static_cast<Tick>(i * 100); b.end = b.begin;
        b.source = SourceId(1); b.source_generation = SourceGeneration(1); b.worker = WorkerId(1); b.worker_boot = WorkerBootId(1);
        b.device = DeviceId(1); b.device_generation = DeviceGeneration(1); b.state = StateCategory::UsefulExecution;
        obs.ingest(b);
        Observation e;
        e.id = ObservationId(i * 2 + 1); e.coordinator_epoch = CoordinatorEpoch(1);
        e.type = ObservationType::ExecutionEnd; e.begin = static_cast<Tick>(i * 100 + 40); e.end = e.begin;
        e.source = SourceId(1); e.source_generation = SourceGeneration(1); e.worker = WorkerId(1); e.worker_boot = WorkerBootId(1);
        e.device = DeviceId(1); e.device_generation = DeviceGeneration(1); e.state = StateCategory::UsefulExecution;
        obs.ingest(e);
    }
    std::string p = "bench_uo.uobs";
    auto t0 = std::chrono::steady_clock::now();
    obs.save(p);
    double save_ms = ms_since(t0);
    UtilizationObservatory l;
    t0 = std::chrono::steady_clock::now();
    l.load(p);
    double load_ms = ms_since(t0);
    t0 = std::chrono::steady_clock::now();
    std::uint64_t d = obs.canonical_digest();
    double dig_ms = ms_since(t0);
    t0 = std::chrono::steady_clock::now();
    ReplayResult rr = obs.replay(p);
    double rp_ms = ms_since(t0);
    std::printf("  persist %-5s: save=%7.2fms load=%7.2fms digest=%6.2fms replay=%6.2fms (equal=%d)\n",
                scale_name(n), save_ms, load_ms, dig_ms, rp_ms, rr.digests_equal ? 1 : 0);
    (void)d;
    std::remove(p.c_str());
}

int main() {
    std::printf("Utilization Observatory benchmarks\n");
    const size_t scales[] = {100, 1000, 10000, 100000};
    for (size_t n : scales) {
        bench_ingest(n);
        bench_query(n);
        bench_persistence(n);
    }
}
