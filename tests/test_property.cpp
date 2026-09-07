#include "test_framework.hpp"
#include "uo/observatory.hpp"
#include "uo/source.hpp"
#include "uo/time.hpp"
#include "uo/view.hpp"

#include <memory>

using namespace uo;

static Observation make(ObservationId id, ObservationType type, Tick b, Tick e,
                        StateCategory cat = StateCategory::Unknown) {
    Observation o;
    o.id = id;
    o.coordinator_epoch = CoordinatorEpoch(1);
    o.type = type;
    o.begin = b; o.end = e;
    o.source = SourceId(1); o.source_generation = SourceGeneration(1);
    o.worker = WorkerId(1); o.worker_boot = WorkerBootId(1);
    o.device = DeviceId(1); o.device_generation = DeviceGeneration(1);
    o.state = cat;
    return o;
}

static UtilizationObservatory build_mixed() {
    UtilizationObservatory obs;
    // busy useful [0,200], retry [200,300], transfer [50,150], reserved [300,400],
    // available idle [400,500], stranded [500,600].
    obs.ingest(make(ObservationId(1), ObservationType::ExecutionBegin, 0, 0, StateCategory::UsefulExecution));
    obs.ingest(make(ObservationId(2), ObservationType::ExecutionEnd, 200, 200, StateCategory::UsefulExecution));
    obs.ingest(make(ObservationId(3), ObservationType::ExecutionBegin, 200, 200, StateCategory::RetryExecution));
    obs.ingest(make(ObservationId(4), ObservationType::ExecutionEnd, 300, 300, StateCategory::RetryExecution));
    obs.ingest(make(ObservationId(5), ObservationType::TransferBegin, 50, 50, StateCategory::TransferActive));
    obs.ingest(make(ObservationId(6), ObservationType::TransferEnd, 150, 150, StateCategory::TransferActive));
    obs.ingest(make(ObservationId(7), ObservationType::ReservationBegin, 300, 300, StateCategory::ReservedIdle));
    obs.ingest(make(ObservationId(8), ObservationType::ReservationEnd, 400, 400, StateCategory::ReservedIdle));
    obs.ingest(make(ObservationId(9), ObservationType::ExecutionBegin, 400, 400, StateCategory::DeviceIdle));
    obs.ingest(make(ObservationId(10), ObservationType::ExecutionEnd, 500, 500, StateCategory::DeviceIdle));
    obs.ingest(make(ObservationId(11), ObservationType::ExecutionBegin, 500, 500, StateCategory::FragmentationStranded));
    obs.ingest(make(ObservationId(12), ObservationType::ExecutionEnd, 600, 600, StateCategory::FragmentationStranded));
    return obs;
}

TEST(property_useful_and_nonuseful_subset_of_busy) {
    auto obs = build_mixed();
    // busy = useful[0,200] + retry[200,300] = 300; useful = 200; nonuseful(retry) = 100.
    auto v = obs.device_window(DeviceId(1), Interval{0, 600});
    CHECK(v.useful_compute_time <= v.compute_busy_time);
    CHECK(v.non_useful_compute_time + v.necessary_overhead_time + v.retry_time <= v.compute_busy_time);
    CHECK_EQ(v.compute_busy_time, 300u);
    CHECK_EQ(v.useful_compute_time, 200u);
    CHECK_EQ(v.retry_time, 100u);
}

TEST(property_reserved_idle_distinct_from_available_idle) {
    // A device that is ONLY reserved-idle must report zero AVAILABLE idle.
    UtilizationObservatory only;
    CHECK(only.ingest(make(ObservationId(1), ObservationType::ReservationBegin, 0, 0, StateCategory::ReservedIdle)).ok());
    CHECK(only.ingest(make(ObservationId(2), ObservationType::ReservationEnd, 100, 100, StateCategory::ReservedIdle)).ok());
    auto v = only.device_window(DeviceId(1), Interval{0, 100});
    CHECK_EQ(v.reserved_idle_time, 100u);
    CHECK_EQ(v.compute_idle_time, 0u);  // reserved idle must NOT become available idle

    auto mixed = build_mixed();
    auto vm = mixed.device_window(DeviceId(1), Interval{0, 600});
    CHECK_EQ(vm.reserved_idle_time, 100u);
    CHECK_EQ(vm.compute_idle_time, 100u);  // available idle, tracked separately
}

TEST(property_stranded_not_consumed) {
    auto obs = build_mixed();
    auto v = obs.device_window(DeviceId(1), Interval{0, 600});
    // stranded time is reported separately from compute_busy.
    CHECK_EQ(v.stranded_capacity_time, 100u);
    std::uint64_t consumed = v.compute_busy_time;
    CHECK(v.stranded_capacity_time != consumed);
}

TEST(property_overlap_not_double_counted) {
    auto obs = build_mixed();
    auto v = obs.device_window(DeviceId(1), Interval{0, 600});
    // transfer [50,150] overlaps useful busy; overlap <= min(busy, transfer)
    CHECK(v.compute_transfer_overlap_time <= v.compute_busy_time);
    CHECK(v.compute_transfer_overlap_time <= v.transfer_active_time);
    CHECK_EQ(v.compute_transfer_overlap_time, 100u);
}

TEST(property_tumbling_windows_additive) {
    auto obs = build_mixed();
    auto a = obs.device_window(DeviceId(1), Interval{0, 100});
    auto b = obs.device_window(DeviceId(1), Interval{100, 200});
    auto full = obs.device_window(DeviceId(1), Interval{0, 200});
    // useful busy is [0,200]; disjoint halves add.
    CHECK_EQ(a.useful_compute_time + b.useful_compute_time, full.useful_compute_time);
    CHECK_EQ(full.useful_compute_time, 200u);
}

TEST(property_duplicate_does_not_change_totals) {
    auto obs = build_mixed();
    auto before = obs.device_window(DeviceId(1), Interval{0, 300});
    // re-ingest a duplicate begin/end (same ids) -> rejected
    CHECK(obs.ingest(make(ObservationId(1), ObservationType::ExecutionBegin, 0, 0, StateCategory::UsefulExecution)).code() == StatusCode::DuplicateObservation);
    CHECK(obs.ingest(make(ObservationId(2), ObservationType::ExecutionEnd, 200, 200, StateCategory::UsefulExecution)).code() == StatusCode::DuplicateObservation);
    auto after = obs.device_window(DeviceId(1), Interval{0, 300});
    CHECK_EQ(before.compute_busy_time, after.compute_busy_time);
    CHECK_EQ(before.useful_compute_time, after.useful_compute_time);
}

TEST(property_timeline_deterministic) {
    auto obs = build_mixed();
    auto t1 = obs.timeline(Interval{0, 600});
    auto t2 = obs.timeline(Interval{0, 600});
    CHECK_EQ(t1.size(), t2.size());
    bool same = true;
    for (size_t i = 0; i < t1.size(); ++i) {
        if (t1[i].span.begin != t2[i].span.begin || t1[i].state != t2[i].state) { same = false; break; }
    }
    CHECK(same);
    // sorted by begin
    for (size_t i = 1; i < t1.size(); ++i) CHECK(t1[i - 1].span.begin <= t1[i].span.begin);
}

TEST(property_unknown_source_does_not_become_zero) {
    UtilizationObservatory obs;
    auto v = obs.device_window(DeviceId(42), Interval{0, 100});
    CHECK_EQ(v.window_span, 100u);
    CHECK_EQ(v.compute_busy_time, 0u);   // no evidence
    // gap unknown ratio is 1.0 (whole window UNKNOWN), not a fabricated zero busy.
    auto g = obs.gap(DeviceId(42), Interval{0, 100});
    CHECK(g.unknown.defined());
    CHECK_NEAR(g.unknown.value(), 1.0, 1e-9);
    CHECK(!g.headline_busy.defined());
}

TEST(property_unsupported_telemetry_is_not_zero) {
    UtilizationObservatory obs;
    // A source advertises no memory-bytes capability. Query must report UNSUPPORTED,
    // not a zero value pretending to be measured.
    SourceInfo si; si.id = SourceId(1); si.generation = SourceGeneration(1);
    si.worker = WorkerId(1); si.boot = WorkerBootId(1);
    si.capabilities.memory_bytes = false;  // explicitly unsupported
    CHECK(obs.register_source(si).ok());
    Observation o = make(ObservationId(1), ObservationType::DeviceSample, 0, 0);
    CHECK(obs.ingest(o).ok());
    auto caps = obs.capabilities(DeviceId(1));
    CHECK(caps.memory_bytes == false);
}
