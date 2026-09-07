#pragma once
// Utilization Observatory : the public API.
// This is the evidence boundary that explains how nominal accelerator capacity
// became useful execution, overhead, waste, waiting, idle, reserved, stranded,
// blocked, or unknown over time. It is vendor-neutral; optional hardware backends
// remain isolated and are not required for the core.
// Copyright 2026 Summon Software Labs. Apache-2.0.

#include "uo/digest.hpp"
#include "uo/ids.hpp"
#include "uo/interval.hpp"
#include "uo/model.hpp"
#include "uo/observation.hpp"
#include "uo/source.hpp"
#include "uo/status.hpp"
#include "uo/summary.hpp"
#include "uo/time.hpp"
#include "uo/view.hpp"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace uo {

// Internal state is fully hidden behind a pimpl so the public header never
// exposes locks, sockets, or implementation details.
class UtilizationObservatory {
public:
    struct Config {
        // Freshness thresholds in monotonic nanoseconds.
        Tick stale_after_ns{1000000000};
        Tick expire_after_ns{5000000000};
        // Retention / source bounds.
        std::size_t max_observations{1000000};
        std::size_t max_sources{4096};
        std::size_t max_devices{1024};
        std::size_t max_open_intervals{4096};
        std::size_t dedup_capacity{1048576};
        std::size_t max_events_per_query{200000};
        // The observatory's own coordinator epoch and generation.
        CoordinatorEpoch epoch{CoordinatorEpoch(1)};
        ObservatoryGeneration generation{ObservatoryGeneration(1)};
    };

    explicit UtilizationObservatory(Config cfg = Config{});
    UtilizationObservatory(Config cfg, std::shared_ptr<MonotonicClock> clock);
    ~UtilizationObservatory();

    UtilizationObservatory(const UtilizationObservatory&) = delete;
    UtilizationObservatory& operator=(const UtilizationObservatory&) = delete;
    UtilizationObservatory(UtilizationObservatory&&) noexcept;
    UtilizationObservatory& operator=(UtilizationObservatory&&) noexcept;

    // Source lifecycle.
    Status register_source(SourceInfo& info);
    Status mark_source_connected(SourceId id, Tick t);
    Status mark_source_disconnected(SourceId id, Tick t);

    // Ingestion.
    Status ingest(const Observation& obs);

    // Queries.
    UtilizationVector device_window(DeviceId dev, Interval win) const;
    UtilizationGap gap(DeviceId dev, Interval win) const;
    UtilizationExplanation explain(DeviceId dev, Interval win) const;
    DeviceSnapshot snapshot(DeviceId dev, Tick t) const;
    WorkloadSnapshot workload(WorkloadId wl, Interval win) const;
    std::vector<TimelineEvent> timeline(Interval win) const;
    CapacityState capacity(DeviceId dev, Tick t) const;
    BackendCapabilities capabilities(DeviceId dev) const;
    SourceInfo source_health(SourceId id) const;
    std::vector<SourceInfo> all_sources() const;
    std::vector<DeviceId> all_devices() const;

    // Persistence / replay / digest.
    Status save(const std::string& path) const;
    Status load(const std::string& path);
    std::uint64_t canonical_digest() const;
    ReplayResult replay(const std::string& path) const;

    // Observability of the observatory.
    struct ObservatoryHealth {
        std::uint64_t accepted{0};
        std::uint64_t rejected{0};
        std::uint64_t duplicates{0};
        std::uint64_t stale{0};
        std::uint64_t source_disconnects{0};
        std::uint64_t source_reconnects{0};
        std::uint64_t persistence_errors{0};
        std::uint64_t backend_errors{0};
        std::uint64_t dropped_bounds{0};
        std::size_t source_count{0};
        std::size_t device_count{0};
    };
    ObservatoryHealth health() const;

    CoordinatorEpoch epoch() const noexcept;
    void advance_epoch();

private:
    Status ingest_locked(const Observation& obs);
    struct Impl;
    std::unique_ptr<Impl> impl_;
    Config cfg_;
};

} // namespace uo
