#pragma once
// Utilization Observatory : utilization source identity, health, and freshness.
// Source identity carries generation and boot authority; source health exposes
// telemetry loss rather than hiding it.
// Copyright 2026 Summon Software Labs. Apache-2.0.

#include "uo/ids.hpp"
#include "uo/model.hpp"
#include "uo/time.hpp"
#include <cstdint>
#include <string>

namespace uo {

struct SourceInfo {
    SourceId id;
    SourceGeneration generation;
    WorkerId worker;
    WorkerBootId boot;
    HostId host;
    HostGeneration host_generation;
    std::string name;
    std::string backend;                 // "nvml", "cuda", "trace", "synthetic", ...
    BackendCapabilities capabilities;
    Freshness freshness{Freshness::Unknown};
    SourceHealth health{SourceHealth::Unknown};
    Tick last_observation{0};
    Tick last_successful_sample{0};
    WallTime last_wall{0};
    std::int32_t error_code{0};
    std::string origin;                  // endpoint or pid identity
};

} // namespace uo
