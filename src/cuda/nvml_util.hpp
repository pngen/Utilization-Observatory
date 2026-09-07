#pragma once
// Utilization Observatory : NVML telemetry wrapper (RTX 5090 proof).
// Copyright 2026 Summon Software Labs. Apache-2.0.
#include <cstdint>

struct NvmlDeviceInfo {
    std::uint64_t memory_total_bytes{0};
    std::uint64_t memory_free_bytes{0};
    int compute_util_percent{-1};
    int mem_util_percent{-1};
    int temperature{-1};
    unsigned power_mW{0};
    char name[64]{0};
    bool supported{false};          // whether compute/memory utilization is queryable
    bool memory_supported{false};
};

bool nvml_init();
void nvml_shutdown();
bool nvml_device_count(int& count);
bool nvml_sample_device(int index, NvmlDeviceInfo& info);
