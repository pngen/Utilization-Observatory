#include "test_framework.hpp"
#include "uo/observatory.hpp"
#include "uo/source.hpp"
#include "uo/time.hpp"
#include "uo/view.hpp"

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

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

static std::string path(const std::string& name) {
    return (std::filesystem::temp_directory_path() / name).string();
}

static void write_all(const std::string& p, const std::vector<char>& bytes) {
    std::ofstream f(p, std::ios::binary | std::ios::trunc);
    f.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

static std::vector<char> read_all(const std::string& p) {
    std::ifstream f(p, std::ios::binary);
    return std::vector<char>((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

TEST(persistence_roundtrip_identical) {
    UtilizationObservatory orig;
    CHECK(orig.ingest(make(ObservationId(1), ObservationType::ExecutionBegin, 0, 0, StateCategory::UsefulExecution)).ok());
    CHECK(orig.ingest(make(ObservationId(2), ObservationType::ExecutionEnd, 100, 100, StateCategory::UsefulExecution)).ok());
    CHECK(orig.ingest(make(ObservationId(3), ObservationType::TransferBegin, 50, 50, StateCategory::TransferActive)).ok());
    CHECK(orig.ingest(make(ObservationId(4), ObservationType::TransferEnd, 150, 150, StateCategory::TransferActive)).ok());
    auto before = orig.device_window(DeviceId(1), Interval{0, 200});
    std::uint64_t d0 = orig.canonical_digest();

    std::string p = path("uo_roundtrip.uobs");
    CHECK(orig.save(p).ok());

    UtilizationObservatory loaded;
    CHECK(loaded.load(p).ok());
    auto after = loaded.device_window(DeviceId(1), Interval{0, 200});
    CHECK_EQ(before.compute_busy_time, after.compute_busy_time);
    CHECK_EQ(before.useful_compute_time, after.useful_compute_time);
    CHECK_EQ(before.transfer_active_time, after.transfer_active_time);
    CHECK_EQ(before.compute_transfer_overlap_time, after.compute_transfer_overlap_time);
    CHECK_EQ(orig.canonical_digest(), loaded.canonical_digest());
    CHECK_EQ(orig.canonical_digest(), d0);
    std::filesystem::remove(p);
}

TEST(persistence_rejects_corruption) {
    UtilizationObservatory obs;
    CHECK(obs.ingest(make(ObservationId(1), ObservationType::ExecutionBegin, 0, 0, StateCategory::UsefulExecution)).ok());
    CHECK(obs.ingest(make(ObservationId(2), ObservationType::ExecutionEnd, 100, 100, StateCategory::UsefulExecution)).ok());
    std::string p = path("uo_corrupt.uobs");
    std::string c = path("uo_corrupt2.uobs");
    CHECK(obs.save(p).ok());
    auto bytes = read_all(p);
    // Flip a byte in the middle of the payload.
    bytes[bytes.size() / 2] ^= 0x5A;
    write_all(c, bytes);
    UtilizationObservatory loaded;
    CHECK(loaded.load(c).code() == StatusCode::PersistenceCorrupt);
    std::filesystem::remove(p);
    std::filesystem::remove(c);
}

TEST(persistence_rejects_truncation) {
    UtilizationObservatory obs;
    CHECK(obs.ingest(make(ObservationId(1), ObservationType::ExecutionBegin, 0, 0, StateCategory::UsefulExecution)).ok());
    CHECK(obs.ingest(make(ObservationId(2), ObservationType::ExecutionEnd, 100, 100, StateCategory::UsefulExecution)).ok());
    std::string p = path("uo_trunc.uobs");
    std::string c = path("uo_trunc2.uobs");
    CHECK(obs.save(p).ok());
    auto bytes = read_all(p);
    bytes.resize(bytes.size() / 2);
    write_all(c, bytes);
    UtilizationObservatory loaded;
    CHECK(loaded.load(c).code() == StatusCode::PersistenceCorrupt);
    std::filesystem::remove(p);
    std::filesystem::remove(c);
}

TEST(persistence_rejects_trailing_garbage) {
    UtilizationObservatory obs;
    CHECK(obs.ingest(make(ObservationId(1), ObservationType::ExecutionBegin, 0, 0, StateCategory::UsefulExecution)).ok());
    std::string p = path("uo_garbage.uobs");
    std::string c = path("uo_garbage2.uobs");
    CHECK(obs.save(p).ok());
    auto bytes = read_all(p);
    bytes.push_back(static_cast<char>(0xDE)); bytes.push_back(static_cast<char>(0xAD)); bytes.push_back(static_cast<char>(0xBE)); bytes.push_back(static_cast<char>(0xEF));
    write_all(c, bytes);
    UtilizationObservatory loaded;
    CHECK(loaded.load(c).code() == StatusCode::PersistenceCorrupt);
    std::filesystem::remove(p);
    std::filesystem::remove(c);
}

TEST(persistence_rejects_unknown_version) {
    UtilizationObservatory obs;
    CHECK(obs.ingest(make(ObservationId(1), ObservationType::ExecutionBegin, 0, 0, StateCategory::UsefulExecution)).ok());
    std::string p = path("uo_version.uobs");
    CHECK(obs.save(p).ok());
    auto bytes = read_all(p);
    bytes[4] = 0x02;  // version = 2 (unknown)
    write_all(p, bytes);
    UtilizationObservatory loaded;
    CHECK(loaded.load(p).code() == StatusCode::PersistenceCorrupt);
    std::filesystem::remove(p);
}

TEST(persistence_replay_identical_digest) {
    UtilizationObservatory obs;
    CHECK(obs.ingest(make(ObservationId(1), ObservationType::ExecutionBegin, 0, 0, StateCategory::UsefulExecution)).ok());
    CHECK(obs.ingest(make(ObservationId(2), ObservationType::ExecutionEnd, 100, 100, StateCategory::UsefulExecution)).ok());
    CHECK(obs.ingest(make(ObservationId(3), ObservationType::ExecutionBegin, 100, 100, StateCategory::RetryExecution)).ok());
    CHECK(obs.ingest(make(ObservationId(4), ObservationType::ExecutionEnd, 200, 200, StateCategory::RetryExecution)).ok());
    std::string p = path("uo_replay.uobs");
    CHECK(obs.save(p).ok());
    ReplayResult res = obs.replay(p);
    CHECK(res.digests_equal);
    CHECK_EQ(res.observations_replayed, 4u);
    std::filesystem::remove(p);
}

TEST(persistence_restart_history_survives_and_revalidation) {
    UtilizationObservatory inc1;
    CHECK(inc1.ingest(make(ObservationId(1), ObservationType::ExecutionBegin, 0, 0, StateCategory::UsefulExecution)).ok());
    CHECK(inc1.ingest(make(ObservationId(2), ObservationType::ExecutionEnd, 100, 100, StateCategory::UsefulExecution)).ok());
    std::uint64_t busy_before = inc1.device_window(DeviceId(1), Interval{0, 200}).compute_busy_time;
    std::string p = path("uo_restart.uobs");
    CHECK(inc1.save(p).ok());

    // Coordinator incarnation 2 with an advanced epoch.
    UtilizationObservatory::Config cfg2;
    cfg2.epoch = CoordinatorEpoch(2);
    UtilizationObservatory inc2(cfg2);
    CHECK(inc2.load(p).ok());

    // Historical timeline survives.
    CHECK_EQ(inc2.device_window(DeviceId(1), Interval{0, 200}).compute_busy_time, busy_before);
    // Dynamic source state requires revalidation after restart.
    auto srcs = inc2.all_sources();
    bool reval = !srcs.empty();
    for (auto& s : srcs) if (s.freshness != Freshness::RevalidationRequired) reval = false;
    CHECK(reval);
    // Old-epoch traffic rejects.
    Observation oldmsg = make(ObservationId(9), ObservationType::DeviceSample, 500, 500);
    oldmsg.coordinator_epoch = CoordinatorEpoch(1);
    CHECK(inc2.ingest(oldmsg).code() == StatusCode::StaleAuthority);
    // Fresh-epoch traffic with a fresh boot re-establishes authority.
    Observation fresh = make(ObservationId(10), ObservationType::DeviceSample, 600, 600);
    fresh.coordinator_epoch = CoordinatorEpoch(2);
    fresh.worker_boot = WorkerBootId(999);
    CHECK(inc2.ingest(fresh).ok());
    std::filesystem::remove(p);
}
