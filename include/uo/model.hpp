#pragma once
// Utilization Observatory : core semantic model.
// Enumerations and small value types for provenance, freshness, source health,
// state categories, idle/busy reasons, observation types, capabilities, and the
// REAL/DERIVED/SYNTHETIC/UNSUPPORTED evidence label.
// Copyright 2026 Summon Software Labs. Apache-2.0.

#include "uo/time.hpp"
#include <cstdint>

namespace uo {

// Whether evidence was measured from the ground truth, reported by a trusted
// upstream, or reconstructed/derived by this runtime. Never blurred.
enum class Provenance {
    Measured, Reported, Derived, Reconstructed, Estimated, Synthetic, Policy, Unknown
};
const char* to_string(Provenance p) noexcept;

// The evidence label used across the product: REAL vs DERIVED vs SYNTHETIC vs
// UNSUPPORTED. UNSUPPORTED is a distinct, explicitly labeled condition, never zero.
enum class EvidenceLabel { Real, Derived, Synthetic, Unsupported };
const char* to_string(EvidenceLabel e) noexcept;

// Freshness of a source's evidence.
enum class Freshness { Current, Stale, Expired, RevalidationRequired, Historical, Unknown };
const char* to_string(Freshness f) noexcept;

// Health of a utilization source.
enum class SourceHealth {
    Healthy, Degraded, Stale, Disconnected, RevalidationRequired, Unsupported, Unknown
};
const char* to_string(SourceHealth h) noexcept;

// How a vendor utilization sample was produced. A 78% sample means different
// things depending on the backend semantics; this is captured explicitly.
enum class SampleSemantics {
    InstantaneousEstimate, AveragedPriorInterval, RollingWindow, BackendOpaque
};
const char* to_string(SampleSemantics s) noexcept;

// Physical device state. Distinct from logical workload state.
enum class DeviceState {
    ComputeActive, ComputeIdle, MemoryResident, Reserved, Fenced, Unavailable, Unknown
};
const char* to_string(DeviceState s) noexcept;

// Logical workload state. Distinct from physical device state.
enum class WorkloadState {
    Running, Queued, WaitingDependency, WaitingTransfer, WaitingMemory, WaitingRecovery,
    Retrying, Done, Failed, Cancelled, Unknown
};
const char* to_string(WorkloadState s) noexcept;

// Explicit utilization state categories. A coherent taxonomy that explains where
// nominal accelerator capacity is going. Not every event is forced into every
// category.
enum class StateCategory {
    UsefulExecution, NonUsefulExecution, NecessaryOverhead, TransferActive, MemoryBound,
    WaitingQueue, WaitingDependency, WaitingTransfer, WaitingMemory, WaitingRecovery,
    RetryExecution, ReservedIdle, ResidencyIdle, FragmentationStranded, AuthorityFenced,
    DeviceIdle, DeviceUnavailable, Unknown
};
const char* to_string(StateCategory c) noexcept;

// Why a device/workload is busy. Busy is not assumed productive.
enum class BusyReason {
    Useful, NecessaryOverhead, AvoidableWaste, Retry, Recomputation, Recovery,
    TransferAssist, UnknownBusy
};
const char* to_string(BusyReason r) noexcept;

// Why capacity is idle. Idle is not assumed wasted.
enum class IdleReason {
    AvailableIdle, ReservedIdle, ResidencyIdle, DependencyBlocked, AuthorityFenced,
    FragmentationStranded, CapacityUnfit, WorkloadStarved, RecoveryWait, UnknownIdle
};
const char* to_string(IdleReason r) noexcept;

// Efficiency-Ledger-compatible classification of physical work. Utilization
// Observatory ingests these; it does not re-derive the policy.
enum class Classification {
    Useful, NecessaryOverhead, AvoidableWaste, AvoidedWork, StrandedCapacity, Unknown
};
const char* to_string(Classification c) noexcept;

// Observation record types. Only implemented events are used.
enum class ObservationType {
    DeviceSample, WorkloadSample, ExecutionBegin, ExecutionEnd, TransferBegin, TransferEnd,
    ReservationBegin, ReservationEnd, ResidencyBegin, ResidencyEnd, WaitBegin, WaitEnd,
    RecoveryBegin, RecoveryEnd, AttemptBegin, AttemptEnd, WorkerReady, WorkerLost,
    ResourceAvailable, ResourceUnavailable, FragmentationObserved, FragmentationCleared,
    CapacityPublished, CapacityInvalidated, UsefulWorkSummary, EfficiencySummary,
    HealthSample, PowerSample, MemorySample
};
const char* to_string(ObservationType t) noexcept;

// Conservative correlation label. Correlation is not causation.
enum class CorrelationKind { TemporallyAssociated, PotentialContributor, UnknownCause };
const char* to_string(CorrelationKind k) noexcept;

// Transfer direction.
enum class TransferDirection { H2D, D2H, P2P, Network, Storage, HostMemory, Unknown };
const char* to_string(TransferDirection d) noexcept;

// One component of physical capacity. Kept distinct: physical, published,
// available, allocated, reserved, resident, fit-qualified, stranded, blocked,
// unknown.
enum class CapacityRole {
    Physical, Published, Available, Allocated, Reserved, Resident, FitQualified,
    Stranded, Blocked, UnknownCapacity
};
const char* to_string(CapacityRole r) noexcept;

// What authority a source carries. Epoch markers are strict; revalidation is
// required whenever the authority domain changes.
enum class AuthorityLevel { Coordinator, Source, Worker, Device, Workload, Evidence, None };
const char* to_string(AuthorityLevel a) noexcept;

// Capabilities exposed by a backend. Each field is independently queryable;
// unsupported fields are UNSUPPORTED, never zero.
struct BackendCapabilities {
    bool device_identity{false};
    bool compute_utilization{false};
    bool memory_utilization{false};
    bool memory_bytes{false};
    bool power{false};
    bool temperature{false};
    bool clocks{false};
    bool per_process_utilization{false};
    bool engine_utilization{false};
    bool kernel_intervals{false};
    bool transfer_intervals{false};
    bool nvlink{false};
    bool rdma{false};
    bool mig{false};
};

} // namespace uo
