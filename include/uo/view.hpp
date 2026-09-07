#pragma once
// Utilization Observatory : coherent snapshots, explanations and replay results.
// Snapshot consistency is explicit; observations from very different times are
// never mixed without marking that.
// Copyright 2026 Summon Software Labs. Apache-2.0.

#include "uo/ids.hpp"
#include "uo/model.hpp"
#include "uo/source.hpp"
#include "uo/summary.hpp"
#include "uo/time.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace uo {

// A coherent device snapshot at a point in time.
struct DeviceSnapshot {
    DeviceId device;
    DeviceGeneration generation;
    SnapshotId id;
    Tick at{0};
    DeviceState state{DeviceState::Unknown};
    UtilizationVector dims;
    CapacityState capacity;
    SourceId source;
    SourceGeneration source_generation;
    Provenance provenance{Provenance::Unknown};
    EvidenceLabel label{EvidenceLabel::Derived};
    Freshness freshness{Freshness::Unknown};
    bool consistent{false};
    double vendor_percent{0.0};
    bool vendor_percent_defined{false};
    WallTime wall_time{0};
};

// A workload-level view where attribution evidence exists. Never fabricated from
// device-level samples alone.
struct WorkloadSnapshot {
    WorkloadId workload;
    WorkloadGeneration generation;
    WorkloadState state{WorkloadState::Unknown};
    Tick active_execution{0};
    Tick queue_wait{0};
    Tick retry_time{0};
    Tick transfer_time{0};
    Tick residency_held{0};
    Tick reservation_held{0};
    Tick useful_execution{0};
    Tick non_useful_execution{0};
    Tick blocked_time{0};
    std::uint64_t bytes{0};
};

// Structured explanation of where capacity went.
struct UtilizationExplanation {
    DeviceId device;
    Interval span;
    double headline_busy{0.0};
    double useful_busy{0.0};
    double non_useful_busy{0.0};
    double necessary_overhead{0.0};
    double unknown_busy{0.0};
    double idle{0.0};
    double reserved_idle{0.0};
    double stranded_capacity{0.0};
    std::vector<Contribution> contributors;
    std::vector<std::string> unknown_reasons;
};

// A source-health snapshot.
struct SourceSnapshot {
    SourceInfo info;
    Tick now{0};
    std::uint64_t accepted{0};
    std::uint64_t rejected{0};
    std::uint64_t duplicates{0};
    std::uint64_t stale{0};
};

// Result of a replay: identical reconstruction of historical state.
struct ReplayResult {
    std::uint64_t observations_replayed{0};
    std::uint64_t intervals_reconstructed{0};
    std::uint64_t digest_a{0};
    std::uint64_t digest_b{0};
    bool digests_equal{false};
    std::string persistence_path;
};

} // namespace uo
