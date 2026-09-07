#include "test_framework.hpp"
#include "uo/observatory.hpp"
#include "uo/source.hpp"
#include "uo/time.hpp"
#include "uo/view.hpp"

#include <atomic>
#include <string>
#include <thread>
#include <vector>

using namespace uo;

TEST(concurrency_ingest_readers_save_exact_totals) {
    const int nthreads = 8;
    const int per_thread = 2000;
    UtilizationObservatory obs;

    std::vector<std::thread> producers;
    std::atomic<std::size_t> ok{0};
    for (int t = 0; t < nthreads; ++t) {
        producers.emplace_back([&, t]() {
            for (int i = 0; i < per_thread; ++i) {
                Observation b;
                b.id = ObservationId(static_cast<std::uint64_t>(t) * 1000000 + i * 2 + 1);
                b.coordinator_epoch = CoordinatorEpoch(1);
                b.type = ObservationType::ExecutionBegin;
                b.begin = static_cast<Tick>(i * 10);
                b.end = b.begin;
                b.source = SourceId(t + 1);
                b.source_generation = SourceGeneration(1);
                b.worker = WorkerId(t + 1);
                b.worker_boot = WorkerBootId(1);
                b.device = DeviceId(t + 1);
                b.device_generation = DeviceGeneration(1);
                b.state = StateCategory::UsefulExecution;
                if (obs.ingest(b).ok()) ++ok;

                Observation e;
                e.id = ObservationId(static_cast<std::uint64_t>(t) * 1000000 + i * 2 + 2);
                e.coordinator_epoch = CoordinatorEpoch(1);
                e.type = ObservationType::ExecutionEnd;
                e.begin = static_cast<Tick>(i * 10 + 8);
                e.end = e.begin;
                e.source = SourceId(t + 1);
                e.source_generation = SourceGeneration(1);
                e.worker = WorkerId(t + 1);
                e.worker_boot = WorkerBootId(1);
                e.device = DeviceId(t + 1);
                e.device_generation = DeviceGeneration(1);
                e.state = StateCategory::UsefulExecution;
                if (obs.ingest(e).ok()) ++ok;
            }
        });
    }

    std::atomic<bool> stop{false};
    std::vector<std::thread> readers;
    for (int r = 0; r < 2; ++r) {
        readers.emplace_back([&]() {
            while (!stop.load(std::memory_order_relaxed)) {
                for (int t = 1; t <= nthreads; ++t) {
                    auto v = obs.device_window(DeviceId(t), Interval{0, 20000});
                    (void)v;
                }
            }
        });
    }

    std::string save_path = std::string("build/concurrency.uobs");
    std::thread saver([&]() {
        for (int i = 0; i < 3; ++i) {
            obs.save(save_path);
        }
    });

    for (auto& th : producers) th.join();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    stop.store(true, std::memory_order_relaxed);
    for (auto& th : readers) th.join();
    saver.join();

    const std::size_t total = static_cast<std::size_t>(nthreads) * per_thread * 2;
    CHECK_EQ(ok.load(), total);
    CHECK_EQ(obs.health().accepted, static_cast<std::uint64_t>(total));
    for (int t = 1; t <= nthreads; ++t) {
        auto v = obs.device_window(DeviceId(t), Interval{0, 20000});
        CHECK_EQ(v.compute_busy_time, static_cast<std::uint64_t>(per_thread) * 8u);
        CHECK_EQ(v.useful_compute_time, static_cast<std::uint64_t>(per_thread) * 8u);
    }
}
