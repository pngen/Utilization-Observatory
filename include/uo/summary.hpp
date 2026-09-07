#pragma once
// Utilization Observatory : aggregate summary and view types.
// Utilization is multidimensional; these types preserve the dimensions rather
// than collapsing to one percentage.
// Copyright 2026 Summon Software Labs. Apache-2.0.

#include "uo/ids.hpp"
#include "uo/model.hpp"
#include "uo/time.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace uo {

// Multidimensional utilization vector. Each component is a duration or byte-time
// and is never silently collapsed.
struct UtilizationVector {
    Tick compute_busy_time{0};
    Tick compute_idle_time{0};
    Tick compute_transfer_overlap_time{0};
    Tick useful_compute_time{0};
    Tick non_useful_compute_time{0};
    Tick necessary_overhead_time{0};
    Tick transfer_active_time{0};
    Tick waiting_transfer_time{0};
    Tick waiting_memory_time{0};
    Tick memory_pressure_time{0};
    Tick queue_wait_time{0};
    Tick dependency_wait_time{0};
    Tick recovery_wait_time{0};
    Tick retry_time{0};
    Tick reserved_idle_time{0};
    Tick residency_idle_time{0};
    Tick fenced_time{0};
    Tick unavailable_time{0};
    Tick stranded_capacity_time{0};
    std::uint64_t memory_occupied_byte_time{0};
    std::uint64_t memory_useful_byte_time{0};
    std::uint64_t memory_idle_byte_time{0};
    std::uint64_t transfer_bytes{0};
    std::uint64_t useful_transfer_bytes{0};
    std::uint64_t wasted_transfer_bytes{0};
    std::uint64_t device_capacity_time{0};
    std::uint64_t usable_capacity_time{0};
    std::uint64_t fit_qualified_capacity_time{0};
    Tick window_span{0};
};

// A ratio with an explicit numerator/denominator. With no denominator the ratio is
// UNKNOWN, never a fabricated zero.
struct Ratio {
    double numerator{0};
    double denominator{0};
    bool defined() const noexcept { return denominator > 0.0; }
    double value() const noexcept { return defined() ? numerator / denominator : 0.0; }
    static Ratio undefined() { return Ratio{0.0, 0.0}; }
};

// Capacity state: physical, published, available, allocated, reserved, resident,
// fit-qualified, stranded, blocked, unknown. Kept distinct.
struct CapacityState {
    std::uint64_t physical_bytes{0};
    std::uint64_t published_bytes{0};
    std::uint64_t available_bytes{0};
    std::uint64_t allocated_bytes{0};
    std::uint64_t reserved_bytes{0};
    std::uint64_t resident_bytes{0};
    std::uint64_t fit_qualified_bytes{0};
    std::uint64_t stranded_bytes{0};
    std::uint64_t blocked_bytes{0};
    std::uint64_t unknown_bytes{0};
};

// A single contributor to a gap or an explanation, with an explicit reason.
struct Contribution {
    StateCategory category{StateCategory::Unknown};
    BusyReason busy{BusyReason::UnknownBusy};
    IdleReason idle{IdleReason::UnknownIdle};
    double ratio{0.0};      // fraction of the relevant total (defined denominator)
    double raw_time{0.0};   // raw duration or byte-time in the source denominator
    std::string source;
    Provenance provenance{Provenance::Unknown};
};

// Headline-vs-useful gap. Every ratio exposes numerator/denominator.
struct UtilizationGap {
    DeviceId device;
    Tick window_begin{0};
    Tick window_end{0};
    Ratio headline_busy{};
    Ratio useful_execution{};
    Ratio non_useful_busy{};
    Ratio overhead{};
    Ratio idle{};
    Ratio reserved_idle{};
    Ratio stranded_capacity{};
    Ratio unknown{};
    double gap_points{0.0};     // (headline - useful) * 100
    std::vector<Contribution> contributors;
};

// Event in a merged multi-source timeline.
struct TimelineEvent {
    Interval span;
    SourceId source;
    SourceGeneration source_generation;
    DeviceId device;
    DeviceGeneration device_generation;
    WorkloadId workload;
    WorkloadGeneration workload_generation;
    StateCategory state{StateCategory::Unknown};
    Provenance provenance{Provenance::Unknown};
    Freshness freshness{Freshness::Unknown};
    ObservationId observation;
    std::vector<ObservationId> related_ids;
};

} // namespace uo
