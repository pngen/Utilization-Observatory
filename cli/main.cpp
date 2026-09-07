// Utilization Observatory : focused CLI.
// Copyright 2026 Summon Software Labs. Apache-2.0.
#include "uo/observatory.hpp"
#include "uo/source.hpp"
#include "uo/time.hpp"
#include "uo/view.hpp"
#include "uo/status.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

using namespace uo;

// Build a deterministic synthetic demonstration dataset if no file is loaded.
static UtilizationObservatory build_demo() {
    UtilizationObservatory obs;
    SourceInfo si;
    si.id = SourceId(1); si.generation = SourceGeneration(1);
    si.worker = WorkerId(1); si.boot = WorkerBootId(1);
    si.name = "demo"; si.backend = "synthetic";
    obs.register_source(si);
    ObservationId n(1);
    auto mk = [&](ObservationType t, Tick b, Tick e, StateCategory c) {
        Observation o;
        o.id = n++;
        o.coordinator_epoch = CoordinatorEpoch(1);
        o.type = t; o.begin = b; o.end = e;
        o.source = SourceId(1); o.source_generation = SourceGeneration(1);
        o.worker = WorkerId(1); o.worker_boot = WorkerBootId(1);
        o.device = DeviceId(1); o.device_generation = DeviceGeneration(1);
        o.state = c;
        obs.ingest(o);
    };
    // A window [100, 2000] ns.
    mk(ObservationType::ExecutionBegin, 100, 100, StateCategory::UsefulExecution);
    mk(ObservationType::ExecutionEnd, 1000, 1000, StateCategory::UsefulExecution);
    mk(ObservationType::ExecutionBegin, 1000, 1000, StateCategory::RetryExecution);
    mk(ObservationType::ExecutionEnd, 1400, 1400, StateCategory::RetryExecution);
    mk(ObservationType::TransferBegin, 700, 700, StateCategory::TransferActive);
    mk(ObservationType::TransferEnd, 1200, 1200, StateCategory::TransferActive);
    mk(ObservationType::ReservationBegin, 1400, 1400, StateCategory::ReservedIdle);
    mk(ObservationType::ReservationEnd, 1800, 1800, StateCategory::ReservedIdle);
    mk(ObservationType::ExecutionBegin, 1800, 1800, StateCategory::DeviceIdle);
    mk(ObservationType::ExecutionEnd, 2000, 2000, StateCategory::DeviceIdle);
    Observation frag;
    frag.id = n++; frag.coordinator_epoch = CoordinatorEpoch(1);
    frag.type = ObservationType::FragmentationObserved; frag.begin = 1500; frag.end = 1500;
    frag.source = SourceId(1); frag.source_generation = SourceGeneration(1);
    frag.worker = WorkerId(1); frag.worker_boot = WorkerBootId(1);
    frag.device = DeviceId(1); frag.device_generation = DeviceGeneration(1);
    frag.bytes = 12ull << 30; frag.capacity_bytes = 32607ull << 20;
    frag.state = StateCategory::FragmentationStranded;
    obs.ingest(frag);
    return obs;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::printf("usage: uo <command> [options]\n"
                    "commands: summary gap snapshot sources source-health timeline device workload idle busy\n"
                    "          reserved stranded digest replay validate-state\n");
        return 1;
    }
    std::string cmd = argv[1];
    UtilizationObservatory obs = build_demo();
    // Optional --file <path>
    for (int i = 2; i + 1 < argc; ++i) {
        if (std::strcmp(argv[i], "--file") == 0) {
            if (!obs.load(argv[i + 1]).ok()) std::printf("warning: could not load '%s'\n", argv[i + 1]);
        }
    }
    Tick win_b = 0, win_e = 2000;
    DeviceId dev(1);

    if (cmd == "summary") {
        auto v = obs.device_window(dev, Interval{win_b, win_e});
        std::printf("device=1 window=[%llu,%llu] span=%llu\n",
                    (unsigned long long)win_b, (unsigned long long)win_e, (unsigned long long)v.window_span);
        std::printf("  busy=%llu useful=%llu non_useful=%llu overhead=%llu retry=%llu memory_bound=%llu\n",
                    (unsigned long long)v.compute_busy_time, (unsigned long long)v.useful_compute_time,
                    (unsigned long long)v.non_useful_compute_time, (unsigned long long)v.necessary_overhead_time,
                    (unsigned long long)v.retry_time, (unsigned long long)v.memory_pressure_time);
        std::printf("  transfer=%llu overlap=%llu idle=%llu reserved_idle=%llu residency_idle=%llu stranded=%llu\n",
                    (unsigned long long)v.transfer_active_time, (unsigned long long)v.compute_transfer_overlap_time,
                    (unsigned long long)v.compute_idle_time, (unsigned long long)v.reserved_idle_time,
                    (unsigned long long)v.residency_idle_time, (unsigned long long)v.stranded_capacity_time);
    } else if (cmd == "gap") {
        auto g = obs.gap(dev, Interval{win_b, win_e});
        std::printf("headline_busy=%.4f useful=%.4f non_useful=%.4f overhead=%.4f gap_points=%.2f\n",
                    g.headline_busy.value(), g.useful_execution.value(), g.non_useful_busy.value(),
                    g.overhead.value(), g.gap_points);
        std::printf("  idle=%.4f reserved_idle=%.4f stranded=%.4f unknown=%.4f\n",
                    g.idle.value(), g.reserved_idle.value(), g.stranded_capacity.value(), g.unknown.value());
    } else if (cmd == "snapshot") {
        auto s = obs.snapshot(dev, 1000);
        std::printf("state=%s freshness=%s vendor=%.1f%%(defined=%d) consistent=%d\n",
                    to_string(s.state), to_string(s.freshness), s.vendor_percent,
                    s.vendor_percent_defined ? 1 : 0, s.consistent ? 1 : 0);
    } else if (cmd == "sources") {
        for (auto& s : obs.all_sources())
            std::printf("source=%llu gen=%llu boot=%llu health=%s freshness=%s backend=%s\n",
                        (unsigned long long)s.id.value(), (unsigned long long)s.generation.value(),
                        (unsigned long long)s.boot.value(), to_string(s.health), to_string(s.freshness),
                        s.backend.c_str());
    } else if (cmd == "source-health") {
        SourceId sid(1);
        for (int i = 2; i + 1 < argc; ++i) if (std::strcmp(argv[i], "--source") == 0) sid = SourceId(std::atoll(argv[i + 1]));
        auto s = obs.source_health(sid);
        std::printf("source=%llu health=%s freshness=%s last_obs=%llu backend=%s\n",
                    (unsigned long long)s.id.value(), to_string(s.health), to_string(s.freshness),
                    (unsigned long long)s.last_observation, s.backend.c_str());
    } else if (cmd == "timeline") {
        auto tl = obs.timeline(Interval{win_b, win_e});
        for (auto& e : tl)
            std::printf("  [%llu,%llu] dev=%llu state=%s prov=%s\n",
                        (unsigned long long)e.span.begin, (unsigned long long)e.span.end,
                        (unsigned long long)e.device.value(), to_string(e.state), to_string(e.provenance));
    } else if (cmd == "device") {
        auto c = obs.capacity(dev, 1000);
        std::printf("capacity physical=%llu allocated=%llu reserved=%llu resident=%llu stranded=%llu fit=%llu\n",
                    (unsigned long long)c.physical_bytes, (unsigned long long)c.allocated_bytes,
                    (unsigned long long)c.reserved_bytes, (unsigned long long)c.resident_bytes,
                    (unsigned long long)c.stranded_bytes, (unsigned long long)c.fit_qualified_bytes);
    } else if (cmd == "workload") {
        auto w = obs.workload(WorkloadId(0), Interval{win_b, win_e});
        std::printf("workload active=%llu useful=%llu non_useful=%llu retry=%llu transfer=%llu\n",
                    (unsigned long long)w.active_execution, (unsigned long long)w.useful_execution,
                    (unsigned long long)w.non_useful_execution, (unsigned long long)w.retry_time,
                    (unsigned long long)w.transfer_time);
    } else if (cmd == "idle" || cmd == "busy" || cmd == "reserved" || cmd == "stranded") {
        auto v = obs.device_window(dev, Interval{win_b, win_e});
        if (cmd == "idle") std::printf("available_idle=%llu\n", (unsigned long long)v.compute_idle_time);
        else if (cmd == "busy") std::printf("busy=%llu useful=%llu\n", (unsigned long long)v.compute_busy_time, (unsigned long long)v.useful_compute_time);
        else if (cmd == "reserved") std::printf("reserved_idle=%llu\n", (unsigned long long)v.reserved_idle_time);
        else std::printf("stranded_capacity=%llu\n", (unsigned long long)v.stranded_capacity_time);
    } else if (cmd == "digest") {
        std::printf("canonical_digest=0x%016llx\n", (unsigned long long)obs.canonical_digest());
    } else if (cmd == "validate-state") {
        auto v = obs.device_window(dev, Interval{win_b, win_e});
        bool ok = v.useful_compute_time <= v.compute_busy_time
               && (v.non_useful_compute_time + v.necessary_overhead_time + v.retry_time) <= v.compute_busy_time
               && v.compute_transfer_overlap_time <= v.compute_busy_time
               && v.compute_transfer_overlap_time <= v.transfer_active_time;
        std::printf("validate-state: %s\n", ok ? "OK" : "FAIL");
        return ok ? 0 : 1;
    } else {
        std::printf("unknown command '%s'\n", cmd.c_str());
        return 1;
    }
    return 0;
}
