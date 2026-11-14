// Regression tests: rejected observations leave generation fences and the
// observation-id set untouched; restart epochs never fall below the persisted one.
#include "test_framework.hpp"
#include "uo/observatory.hpp"
#include "uo/source.hpp"
#include "uo/time.hpp"
#include "uo/view.hpp"

#include <cmath>
#include <cstdio>
#include <limits>
#include <string>

using namespace uo;

static Observation reg_make(std::uint64_t id, std::uint64_t device_gen, std::uint64_t epoch = 1) {
    Observation o;
    o.id = ObservationId(id);
    o.coordinator_epoch = CoordinatorEpoch(epoch);
    o.type = ObservationType::DeviceSample;
    o.begin = 100 + id; o.end = 100 + id;
    o.source = SourceId(1); o.source_generation = SourceGeneration(1);
    o.worker = WorkerId(1); o.worker_boot = WorkerBootId(1);
    o.device = DeviceId(1); o.device_generation = DeviceGeneration(device_gen);
    o.percent = 50.0;
    return o;
}

TEST(regression_malformed_observation_does_not_advance_generation) {
    UtilizationObservatory obs;
    CHECK(obs.ingest(reg_make(1, 1)).ok());
    Observation bad = reg_make(2, 9);
    bad.ratio = std::numeric_limits<double>::quiet_NaN();
    CHECK(obs.ingest(bad).code() == StatusCode::InvalidInput);
    CHECK(obs.ingest(reg_make(3, 1)).ok());     // current generation still accepted
    CHECK(obs.ingest(reg_make(2, 1)).ok());     // the corrected resend of id 2 is not a duplicate
}

TEST(regression_duplicate_observation_does_not_advance_generation) {
    UtilizationObservatory obs;
    CHECK(obs.ingest(reg_make(1, 1)).ok());
    CHECK(obs.ingest(reg_make(1, 7)).code() == StatusCode::DuplicateObservation);
    CHECK(obs.ingest(reg_make(4, 1)).ok());     // generation 1 is still current
}

TEST(regression_restart_epoch_moves_past_persisted_epoch) {
    const std::string p = "uo_regression_epoch.uobs";
    {
        UtilizationObservatory inc1;           // epoch 1
        inc1.advance_epoch();                  // an earlier restart: epoch 2
        CHECK(inc1.ingest(reg_make(1, 1, 2)).ok());
        CHECK(inc1.save(p).ok());
    }
    UtilizationObservatory inc2;               // next incarnation constructed at the default epoch
    CHECK(inc2.load(p).ok());
    inc2.advance_epoch();                      // the restart path advances after loading
    CHECK(inc2.epoch().value() > 2);
    CHECK(inc2.ingest(reg_make(5, 1, 2)).code() == StatusCode::StaleAuthority);
    std::remove(p.c_str());
}
