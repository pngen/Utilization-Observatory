#include "test_framework.hpp"
#include "uo/observatory.hpp"
#include "uo/source.hpp"
#include "uo/time.hpp"
#include "uo/view.hpp"

#include <memory>

using namespace uo;

static Observation make_obs(ObservationId id, ObservationType type, Tick begin, Tick end,
                            StateCategory state = StateCategory::Unknown,
                            Classification cls = Classification::Unknown) {
    Observation o;
    o.id = id;
    o.coordinator_epoch = CoordinatorEpoch(1);
    o.type = type;
    o.begin = begin;
    o.end = end;
    o.source = SourceId(1);
    o.source_generation = SourceGeneration(1);
    o.worker = WorkerId(1);
    o.worker_boot = WorkerBootId(1);
    o.device = DeviceId(1);
    o.device_generation = DeviceGeneration(1);
    o.state = state;
    o.classification = cls;
    o.provenance = Provenance::Reported;
    o.evidence_label = EvidenceLabel::Derived;
    return o;
}

TEST(execution_interval_busy_and_useful) {
    UtilizationObservatory obs;
    CHECK(obs.ingest(make_obs(ObservationId(1), ObservationType::ExecutionBegin, 100, 100,
                              StateCategory::UsefulExecution)).ok());
    CHECK(obs.ingest(make_obs(ObservationId(2), ObservationType::ExecutionEnd, 200, 200,
                              StateCategory::UsefulExecution)).ok());
    auto v = obs.device_window(DeviceId(1), Interval{100, 200});
    CHECK_EQ(v.compute_busy_time, 100u);
    CHECK_EQ(v.useful_compute_time, 100u);
    CHECK_EQ(v.non_useful_compute_time, 0u);
    auto g = obs.gap(DeviceId(1), Interval{100, 200});
    CHECK_NEAR(g.headline_busy.value(), 1.0, 1e-9);
    CHECK_NEAR(g.useful_execution.value(), 1.0, 1e-9);
    CHECK_NEAR(g.gap_points, 0.0, 1e-9);
}

TEST(overlapping_intervals_not_double_counted) {
    UtilizationObservatory obs;
    CHECK(obs.ingest(make_obs(ObservationId(1), ObservationType::ExecutionBegin, 100, 100, StateCategory::UsefulExecution)).ok());
    CHECK(obs.ingest(make_obs(ObservationId(2), ObservationType::ExecutionEnd, 200, 200, StateCategory::UsefulExecution)).ok());
    CHECK(obs.ingest(make_obs(ObservationId(3), ObservationType::ExecutionBegin, 150, 150, StateCategory::UsefulExecution)).ok());
    CHECK(obs.ingest(make_obs(ObservationId(4), ObservationType::ExecutionEnd, 250, 250, StateCategory::UsefulExecution)).ok());
    auto v = obs.device_window(DeviceId(1), Interval{100, 250});
    CHECK_EQ(v.compute_busy_time, 150u);
    CHECK_EQ(v.useful_compute_time, 150u);
}

TEST(transfer_overlap_accounted_once) {
    UtilizationObservatory obs;
    CHECK(obs.ingest(make_obs(ObservationId(1), ObservationType::ExecutionBegin, 100, 100, StateCategory::UsefulExecution)).ok());
    CHECK(obs.ingest(make_obs(ObservationId(2), ObservationType::ExecutionEnd, 300, 300, StateCategory::UsefulExecution)).ok());
    CHECK(obs.ingest(make_obs(ObservationId(3), ObservationType::TransferBegin, 150, 150, StateCategory::TransferActive)).ok());
    CHECK(obs.ingest(make_obs(ObservationId(4), ObservationType::TransferEnd, 250, 250, StateCategory::TransferActive)).ok());
    auto v = obs.device_window(DeviceId(1), Interval{100, 300});
    CHECK_EQ(v.compute_busy_time, 200u);
    CHECK_EQ(v.transfer_active_time, 100u);
    CHECK_EQ(v.compute_transfer_overlap_time, 100u);
}

TEST(duplicate_observation_rejected_and_not_double_counted) {
    UtilizationObservatory obs;
    Observation b = make_obs(ObservationId(1), ObservationType::ExecutionBegin, 100, 100, StateCategory::UsefulExecution);
    CHECK(obs.ingest(b).ok());
    CHECK(obs.ingest(b).code() == StatusCode::DuplicateObservation);
    CHECK(obs.ingest(make_obs(ObservationId(2), ObservationType::ExecutionEnd, 200, 200, StateCategory::UsefulExecution)).ok());
    CHECK(obs.ingest(make_obs(ObservationId(2), ObservationType::ExecutionEnd, 200, 200, StateCategory::UsefulExecution)).code() == StatusCode::DuplicateObservation);
    auto v = obs.device_window(DeviceId(1), Interval{100, 200});
    CHECK_EQ(v.compute_busy_time, 100u);
}

TEST(stale_epoch_rejected) {
    UtilizationObservatory obs;
    Observation o = make_obs(ObservationId(1), ObservationType::DeviceSample, 100, 100);
    o.coordinator_epoch = CoordinatorEpoch(99);
    CHECK(obs.ingest(o).code() == StatusCode::StaleAuthority);
}

TEST(stale_boot_rejected_then_fresh_boot_accepts) {
    UtilizationObservatory obs;
    SourceInfo si;
    si.id = SourceId(1); si.generation = SourceGeneration(1);
    si.worker = WorkerId(1); si.boot = WorkerBootId(1);
    CHECK(obs.register_source(si).ok());
    CHECK(obs.ingest(make_obs(ObservationId(1), ObservationType::DeviceSample, 100, 100)).ok());
    Observation stale = make_obs(ObservationId(2), ObservationType::DeviceSample, 200, 200);
    stale.worker_boot = WorkerBootId(77);
    CHECK(obs.ingest(stale).code() == StatusCode::StaleAuthority);
    CHECK(obs.mark_source_disconnected(SourceId(1), 250).ok());
    Observation fresh = make_obs(ObservationId(3), ObservationType::DeviceSample, 300, 300);
    fresh.worker_boot = WorkerBootId(78);
    CHECK(obs.ingest(fresh).ok());
    Observation good = make_obs(ObservationId(4), ObservationType::DeviceSample, 350, 350);
    good.worker_boot = WorkerBootId(78);
    CHECK(obs.ingest(good).ok());
}

TEST(source_disconnected_rejects) {
    UtilizationObservatory obs;
    SourceInfo si; si.id = SourceId(1); si.generation = SourceGeneration(1);
    si.worker = WorkerId(1); si.boot = WorkerBootId(1);
    CHECK(obs.register_source(si).ok());
    CHECK(obs.mark_source_disconnected(SourceId(1), 100).ok());
    CHECK(obs.ingest(make_obs(ObservationId(1), ObservationType::DeviceSample, 100, 100)).code() == StatusCode::SourceDisconnected);
}

TEST(gap_with_retry_and_useful) {
    UtilizationObservatory obs;
    CHECK(obs.ingest(make_obs(ObservationId(1), ObservationType::ExecutionBegin, 100, 100, StateCategory::UsefulExecution)).ok());
    CHECK(obs.ingest(make_obs(ObservationId(2), ObservationType::ExecutionEnd, 200, 200, StateCategory::UsefulExecution)).ok());
    CHECK(obs.ingest(make_obs(ObservationId(3), ObservationType::ExecutionBegin, 200, 200, StateCategory::RetryExecution)).ok());
    CHECK(obs.ingest(make_obs(ObservationId(4), ObservationType::ExecutionEnd, 300, 300, StateCategory::RetryExecution)).ok());
    auto g = obs.gap(DeviceId(1), Interval{100, 300});
    CHECK_NEAR(g.headline_busy.value(), 1.0, 1e-9);
    CHECK_NEAR(g.useful_execution.value(), 0.5, 1e-9);
    CHECK_NEAR(g.gap_points, 50.0, 1e-9);
    CHECK_NEAR(g.non_useful_busy.value(), 0.5, 1e-9);
}

TEST(reserved_vs_idle_vs_stranded_distinct) {
    UtilizationObservatory obs;
    CHECK(obs.ingest(make_obs(ObservationId(1), ObservationType::ReservationBegin, 100, 100, StateCategory::ReservedIdle)).ok());
    CHECK(obs.ingest(make_obs(ObservationId(2), ObservationType::ReservationEnd, 200, 200, StateCategory::ReservedIdle)).ok());
    CHECK(obs.ingest(make_obs(ObservationId(3), ObservationType::ExecutionBegin, 200, 200, StateCategory::DeviceIdle)).ok());
    CHECK(obs.ingest(make_obs(ObservationId(4), ObservationType::ExecutionEnd, 300, 300, StateCategory::DeviceIdle)).ok());
    CHECK(obs.ingest(make_obs(ObservationId(5), ObservationType::ExecutionBegin, 300, 300, StateCategory::FragmentationStranded)).ok());
    CHECK(obs.ingest(make_obs(ObservationId(6), ObservationType::ExecutionEnd, 400, 400, StateCategory::FragmentationStranded)).ok());
    auto v = obs.device_window(DeviceId(1), Interval{100, 400});
    CHECK_EQ(v.reserved_idle_time, 100u);
    CHECK_EQ(v.compute_idle_time, 100u);
    CHECK_EQ(v.stranded_capacity_time, 100u);
    auto g = obs.gap(DeviceId(1), Interval{100, 400});
    CHECK_NEAR(g.reserved_idle.value(), 1.0 / 3.0, 1e-9);
    CHECK_NEAR(g.stranded_capacity.value(), 1.0 / 3.0, 1e-9);
    CHECK_NEAR(g.idle.value(), 1.0 / 3.0, 1e-9);
}

TEST(capacity_and_fragmentation_evidence) {
    UtilizationObservatory obs;
    Observation mem = make_obs(ObservationId(1), ObservationType::MemorySample, 100, 100);
    mem.bytes = 28799ULL * 1024 * 1024;
    mem.capacity_bytes = 32607ULL * 1024 * 1024;
    CHECK(obs.ingest(mem).ok());
    Observation cap = make_obs(ObservationId(2), ObservationType::CapacityPublished, 100, 100);
    cap.capacity_bytes = 32607ULL * 1024 * 1024;
    CHECK(obs.ingest(cap).ok());
    Observation frag = make_obs(ObservationId(3), ObservationType::FragmentationObserved, 100, 100);
    frag.bytes = 12ULL * 1024 * 1024 * 1024;
    frag.capacity_bytes = 32607ULL * 1024 * 1024;
    CHECK(obs.ingest(frag).ok());
    auto c = obs.capacity(DeviceId(1), 200);
    CHECK(c.physical_bytes > 0);
    CHECK_EQ(c.stranded_bytes, 12ULL * 1024 * 1024 * 1024);
    CHECK(obs.capabilities(DeviceId(1)).memory_bytes == false);
}

TEST(snapshot_freshness_after_work) {
    auto clk = std::make_shared<ScriptedClock>(0);
    UtilizationObservatory::Config cfg;
    cfg.stale_after_ns = 100;
    cfg.expire_after_ns = 500;
    UtilizationObservatory obs(cfg, clk);
    clk->set(1000);
    CHECK(obs.ingest(make_obs(ObservationId(1), ObservationType::DeviceSample, 1000, 1000)).ok());
    auto snap = obs.snapshot(DeviceId(1), 1000);
    CHECK(snap.freshness == Freshness::Current);
    CHECK(snap.consistent);
    clk->set(5000);
    auto snap2 = obs.snapshot(DeviceId(1), 1000);
    CHECK(snap2.freshness != Freshness::Current);
    CHECK(!snap2.consistent);
}

TEST(missing_sample_not_zero_utilization) {
    UtilizationObservatory obs;
    auto v = obs.device_window(DeviceId(99), Interval{0, 1000});
    CHECK_EQ(v.compute_busy_time, 0u);
    auto g = obs.gap(DeviceId(99), Interval{0, 1000});
    CHECK_NEAR(g.unknown.value(), 1.0, 1e-9);
}
