#include "test_framework.hpp"
#include "uo/observatory.hpp"
#include "uo/source.hpp"
#include "uo/time.hpp"
#include "uo/view.hpp"

#include <limits>

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

TEST(adversarial_malformed_inverted_interval) {
    UtilizationObservatory obs;
    Observation o = make(ObservationId(1), ObservationType::ExecutionBegin, 100, 50);
    CHECK(obs.ingest(o).code() == StatusCode::InvalidInput);
}

TEST(adversarial_malformed_bad_ratio_and_percent) {
    UtilizationObservatory obs;
    Observation r = make(ObservationId(1), ObservationType::DeviceSample, 0, 0);
    r.ratio = 1.5;  // > 1
    CHECK(obs.ingest(r).code() == StatusCode::InvalidInput);
    Observation p = make(ObservationId(2), ObservationType::DeviceSample, 0, 0);
    p.percent = 150.0;  // > 100
    CHECK(obs.ingest(p).code() == StatusCode::InvalidInput);
    Observation n = make(ObservationId(3), ObservationType::DeviceSample, 0, 0);
    n.ratio = std::numeric_limits<double>::quiet_NaN();
    CHECK(obs.ingest(n).code() == StatusCode::InvalidInput);
}

TEST(adversarial_out_of_order_end_then_begin) {
    UtilizationObservatory obs;
    // Orphan END first: accepted but yields no interval.
    CHECK(obs.ingest(make(ObservationId(1), ObservationType::ExecutionEnd, 200, 200, StateCategory::UsefulExecution)).ok());
    CHECK(obs.ingest(make(ObservationId(2), ObservationType::ExecutionBegin, 100, 100, StateCategory::UsefulExecution)).ok());
    CHECK(obs.ingest(make(ObservationId(3), ObservationType::ExecutionEnd, 200, 200, StateCategory::UsefulExecution)).ok());
    auto v = obs.device_window(DeviceId(1), Interval{50, 300});
    CHECK_EQ(v.compute_busy_time, 100u);   // only the matched begin/end pair
}

TEST(adversarial_duplicate_end_not_double_counted) {
    UtilizationObservatory obs;
    CHECK(obs.ingest(make(ObservationId(1), ObservationType::ExecutionBegin, 100, 100, StateCategory::UsefulExecution)).ok());
    CHECK(obs.ingest(make(ObservationId(2), ObservationType::ExecutionEnd, 200, 200, StateCategory::UsefulExecution)).ok());
    // Second END with a different id (not a duplicate id) -> orphan, no double count.
    CHECK(obs.ingest(make(ObservationId(3), ObservationType::ExecutionEnd, 200, 200, StateCategory::UsefulExecution)).ok());
    auto v = obs.device_window(DeviceId(1), Interval{50, 300});
    CHECK_EQ(v.compute_busy_time, 100u);
}

TEST(adversarial_open_interval_never_closed) {
    UtilizationObservatory obs;
    CHECK(obs.ingest(make(ObservationId(1), ObservationType::ExecutionBegin, 100, 100, StateCategory::UsefulExecution)).ok());
    // No END. The interval is open; no phantom duration is attributed for a closed window.
    auto v = obs.device_window(DeviceId(1), Interval{100, 200});
    CHECK_EQ(v.compute_busy_time, 0u);
}

TEST(adversarial_late_sample_accepted) {
    UtilizationObservatory obs;
    // A sample with an old timestamp arriving after a newer one is still valid history.
    CHECK(obs.ingest(make(ObservationId(1), ObservationType::DeviceSample, 500, 500)).ok());
    CHECK(obs.ingest(make(ObservationId(2), ObservationType::DeviceSample, 50, 50)).ok());
    auto snap = obs.snapshot(DeviceId(1), 500);
    CHECK(snap.vendor_percent_defined);
}

TEST(adversarial_stale_evidence_generation_rejected) {
    UtilizationObservatory obs;
    Observation o = make(ObservationId(1), ObservationType::DeviceSample, 0, 0);
    o.evidence = EvidenceId(1);
    o.evidence_generation = EvidenceGeneration(2);
    CHECK(obs.ingest(o).ok());
    Observation stale = make(ObservationId(2), ObservationType::DeviceSample, 10, 10);
    stale.evidence = EvidenceId(1);
    stale.evidence_generation = EvidenceGeneration(1);  // lower than current
    CHECK(obs.ingest(stale).code() == StatusCode::StaleEvidence);
}

TEST(adversarial_stale_device_generation_rejected) {
    UtilizationObservatory obs;
    Observation o = make(ObservationId(1), ObservationType::DeviceSample, 0, 0);
    o.device_generation = DeviceGeneration(3);
    CHECK(obs.ingest(o).ok());
    Observation stale = make(ObservationId(2), ObservationType::DeviceSample, 10, 10);
    stale.device_generation = DeviceGeneration(2);  // lower
    CHECK(obs.ingest(stale).code() == StatusCode::StaleEvidence);
}

TEST(adversarial_all_busy_wasted_device) {
    UtilizationObservatory obs;
    // 100% busy but classified as non-useful (waste). Useful must be 0.
    CHECK(obs.ingest(make(ObservationId(1), ObservationType::ExecutionBegin, 0, 0, StateCategory::NonUsefulExecution)).ok());
    CHECK(obs.ingest(make(ObservationId(2), ObservationType::ExecutionEnd, 100, 100, StateCategory::NonUsefulExecution)).ok());
    auto g = obs.gap(DeviceId(1), Interval{0, 100});
    CHECK_NEAR(g.headline_busy.value(), 1.0, 1e-9);
    CHECK_NEAR(g.useful_execution.value(), 0.0, 1e-9);
    CHECK_NEAR(g.non_useful_busy.value(), 1.0, 1e-9);
    CHECK_NEAR(g.gap_points, 100.0, 1e-9);
}
