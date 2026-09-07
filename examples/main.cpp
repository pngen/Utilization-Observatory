// Utilization Observatory : runnable examples (the scenarios in the spec).
// Copyright 2026 Summon Software Labs. Apache-2.0.
#include "uo/observatory.hpp"
#include "uo/source.hpp"
#include "uo/time.hpp"
#include "uo/view.hpp"
#include <cstdio>
#include <string>

using namespace uo;

static Observation make(ObservationId id, ObservationType t, Tick b, Tick e, StateCategory c) {
    Observation o;
    o.id = id; o.coordinator_epoch = CoordinatorEpoch(1); o.type = t;
    o.begin = b; o.end = e;
    o.source = SourceId(1); o.source_generation = SourceGeneration(1);
    o.worker = WorkerId(1); o.worker_boot = WorkerBootId(1);
    o.device = DeviceId(1); o.device_generation = DeviceGeneration(1);
    o.state = c;
    return o;
}

int main() {
    std::printf("Utilization Observatory examples\n");

    {
        UtilizationObservatory o;
        o.ingest(make(ObservationId(1), ObservationType::ExecutionBegin, 0, 0, StateCategory::UsefulExecution));
        o.ingest(make(ObservationId(2), ObservationType::ExecutionEnd, 100, 100, StateCategory::UsefulExecution));
        auto v = o.device_window(DeviceId(1), Interval{0, 100});
        std::printf("[example] basic device utilization: busy=%llu ns useful=%llu ns\n",
                    (unsigned long long)v.compute_busy_time, (unsigned long long)v.useful_compute_time);
    }
    {
        UtilizationObservatory o;
        o.ingest(make(ObservationId(1), ObservationType::ExecutionBegin, 0, 0, StateCategory::UsefulExecution));
        o.ingest(make(ObservationId(2), ObservationType::ExecutionEnd, 100, 100, StateCategory::UsefulExecution));
        o.ingest(make(ObservationId(3), ObservationType::ExecutionBegin, 100, 100, StateCategory::RetryExecution));
        o.ingest(make(ObservationId(4), ObservationType::ExecutionEnd, 200, 200, StateCategory::RetryExecution));
        auto g = o.gap(DeviceId(1), Interval{0, 200});
        std::printf("[example] headline vs useful: headline=%.2f%% useful=%.2f%% gap=%.2f pts\n",
                    g.headline_busy.value() * 100.0, g.useful_execution.value() * 100.0, g.gap_points);
    }
    {
        UtilizationObservatory o;
        o.ingest(make(ObservationId(1), ObservationType::ReservationBegin, 0, 0, StateCategory::ReservedIdle));
        o.ingest(make(ObservationId(2), ObservationType::ReservationEnd, 100, 100, StateCategory::ReservedIdle));
        auto v = o.device_window(DeviceId(1), Interval{0, 100});
        std::printf("[example] idle reservation: reserved_idle=%llu ns (distinct from available idle)\n",
                    (unsigned long long)v.reserved_idle_time);
    }
    {
        UtilizationObservatory o;
        o.ingest(make(ObservationId(1), ObservationType::ResidencyBegin, 0, 0, StateCategory::ResidencyIdle));
        o.ingest(make(ObservationId(2), ObservationType::ResidencyEnd, 100, 100, StateCategory::ResidencyIdle));
        auto v = o.device_window(DeviceId(1), Interval{0, 100});
        std::printf("[example] idle residency: residency_idle=%llu ns, compute_idle=%llu ns\n",
                    (unsigned long long)v.residency_idle_time, (unsigned long long)v.compute_idle_time);
    }
    {
        UtilizationObservatory o;
        Observation f = make(ObservationId(1), ObservationType::FragmentationObserved, 0, 0, StateCategory::FragmentationStranded);
        f.bytes = 12ull << 30; f.capacity_bytes = 32607ull << 20;
        o.ingest(f);
        auto c = o.capacity(DeviceId(1), 0);
        std::printf("[example] fragmentation-stranded: stranded=%llu bytes, free-but-unusable capacity\n",
                    (unsigned long long)c.stranded_bytes);
    }
    {
        UtilizationObservatory o;
        o.ingest(make(ObservationId(1), ObservationType::TransferBegin, 0, 0, StateCategory::TransferActive));
        o.ingest(make(ObservationId(2), ObservationType::TransferEnd, 100, 100, StateCategory::TransferActive));
        auto v = o.device_window(DeviceId(1), Interval{0, 100});
        std::printf("[example] transfer-heavy: transfer_active=%llu ns (distinct from compute)\n",
                    (unsigned long long)v.transfer_active_time);
    }
    {
        auto clk = std::make_shared<ScriptedClock>(0);
        UtilizationObservatory::Config cfg;
        cfg.stale_after_ns = 100; cfg.expire_after_ns = 500;
        UtilizationObservatory o(cfg, clk);
        clk->set(1000);
        o.ingest(make(ObservationId(1), ObservationType::DeviceSample, 1000, 1000, StateCategory::Unknown));
        auto s = o.snapshot(DeviceId(1), 1000);
        std::printf("[example] source freshness current: %s; after stale delay: ", to_string(s.freshness));
        clk->set(5000);
        auto s2 = o.snapshot(DeviceId(1), 1000);
        std::printf("%s (revalidation required)\n", to_string(s2.freshness));
    }
    {
        UtilizationObservatory o;
        auto v = o.device_window(DeviceId(1), Interval{0, 100});
        auto g = o.gap(DeviceId(1), Interval{0, 100});
        std::printf("[example] unknown source state: busy=0 but unknown ratio=%.2f (not fabricating zero)\n", g.unknown.value());
        (void)v;
    }
    {
        // Multi-source timeline.
        UtilizationObservatory o;
        Observation o1 = make(ObservationId(1), ObservationType::ExecutionBegin, 0, 0, StateCategory::UsefulExecution);
        o1.source = SourceId(1); o.ingest(o1);
        Observation o2 = make(ObservationId(2), ObservationType::ExecutionBegin, 50, 50, StateCategory::UsefulExecution);
        o2.source = SourceId(2); o2.worker = WorkerId(2); o.ingest(o2);
        Observation o3 = make(ObservationId(3), ObservationType::ExecutionEnd, 100, 100, StateCategory::UsefulExecution);
        o3.source = SourceId(1); o.ingest(o3);
        auto tl = o.timeline(Interval{0, 100});
        std::printf("[example] multi-source timeline: %llu events\n", (unsigned long long)tl.size());
    }
    {
        // Unsupported telemetry remains UNSUPPORTED.
        UtilizationObservatory o;
        SourceInfo si; si.id = SourceId(1); si.generation = SourceGeneration(1);
        si.worker = WorkerId(1); si.boot = WorkerBootId(1);
        si.capabilities.power = false;
        o.register_source(si);
        o.ingest(make(ObservationId(1), ObservationType::DeviceSample, 0, 0, StateCategory::Unknown));
        auto caps = o.capabilities(DeviceId(1));
        std::printf("[example] unsupported telemetry: power=%d (UNSUPPORTED, not zero)\n", caps.power ? 1 : 0);
    }
    {
        // Persistence + replay example.
        UtilizationObservatory o;
        o.ingest(make(ObservationId(1), ObservationType::ExecutionBegin, 0, 0, StateCategory::UsefulExecution));
        o.ingest(make(ObservationId(2), ObservationType::ExecutionEnd, 100, 100, StateCategory::UsefulExecution));
        std::string p = "examples/example.uobs";
        o.save(p);
        ReplayResult rr = o.replay(p);
        std::printf("[example] historical replay: digest_equal=%d replayed=%llu\n",
                    rr.digests_equal ? 1 : 0, (unsigned long long)rr.observations_replayed);
        std::remove(p.c_str());
    }
    std::printf("examples complete\n");
    return 0;
}
