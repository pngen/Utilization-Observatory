#include "uo/model.hpp"
#include "uo/status.hpp"

namespace uo {

const char* to_string(StatusCode c) noexcept {
    switch (c) {
        case StatusCode::Ok: return "OK";
        case StatusCode::InvalidInput: return "INVALID_INPUT";
        case StatusCode::DuplicateObservation: return "DUPLICATE_OBSERVATION";
        case StatusCode::StaleAuthority: return "STALE_AUTHORITY";
        case StatusCode::StaleSource: return "STALE_SOURCE";
        case StatusCode::StaleEvidence: return "STALE_EVIDENCE";
        case StatusCode::SourceDisconnected: return "SOURCE_DISCONNECTED";
        case StatusCode::Unsupported: return "UNSUPPORTED";
        case StatusCode::InsufficientEvidence: return "INSUFFICIENT_EVIDENCE";
        case StatusCode::UnknownState: return "UNKNOWN_STATE";
        case StatusCode::PersistenceCorrupt: return "PERSISTENCE_CORRUPT";
        case StatusCode::ProtocolError: return "PROTOCOL_ERROR";
        case StatusCode::Overflow: return "OVERFLOW";
        case StatusCode::ResourceExhausted: return "RESOURCE_EXHAUSTED";
        case StatusCode::Cancelled: return "CANCELLED";
        case StatusCode::ShuttingDown: return "SHUTTING_DOWN";
    }
    return "UNKNOWN";
}

const char* to_string(Provenance p) noexcept {
    switch (p) {
        case Provenance::Measured: return "MEASURED";
        case Provenance::Reported: return "REPORTED";
        case Provenance::Derived: return "DERIVED";
        case Provenance::Reconstructed: return "RECONSTRUCTED";
        case Provenance::Estimated: return "ESTIMATED";
        case Provenance::Synthetic: return "SYNTHETIC";
        case Provenance::Policy: return "POLICY";
        case Provenance::Unknown: return "UNKNOWN";
    }
    return "UNKNOWN";
}

const char* to_string(EvidenceLabel e) noexcept {
    switch (e) {
        case EvidenceLabel::Real: return "REAL";
        case EvidenceLabel::Derived: return "DERIVED";
        case EvidenceLabel::Synthetic: return "SYNTHETIC";
        case EvidenceLabel::Unsupported: return "UNSUPPORTED";
    }
    return "UNKNOWN";
}

const char* to_string(Freshness f) noexcept {
    switch (f) {
        case Freshness::Current: return "CURRENT";
        case Freshness::Stale: return "STALE";
        case Freshness::Expired: return "EXPIRED";
        case Freshness::RevalidationRequired: return "REVALIDATION_REQUIRED";
        case Freshness::Historical: return "HISTORICAL";
        case Freshness::Unknown: return "UNKNOWN";
    }
    return "UNKNOWN";
}

const char* to_string(SourceHealth h) noexcept {
    switch (h) {
        case SourceHealth::Healthy: return "HEALTHY";
        case SourceHealth::Degraded: return "DEGRADED";
        case SourceHealth::Stale: return "STALE";
        case SourceHealth::Disconnected: return "DISCONNECTED";
        case SourceHealth::RevalidationRequired: return "REVALIDATION_REQUIRED";
        case SourceHealth::Unsupported: return "UNSUPPORTED";
        case SourceHealth::Unknown: return "UNKNOWN";
    }
    return "UNKNOWN";
}

const char* to_string(SampleSemantics s) noexcept {
    switch (s) {
        case SampleSemantics::InstantaneousEstimate: return "INSTANTANEOUS_ESTIMATE";
        case SampleSemantics::AveragedPriorInterval: return "AVERAGED_PRIOR_INTERVAL";
        case SampleSemantics::RollingWindow: return "ROLLING_WINDOW";
        case SampleSemantics::BackendOpaque: return "BACKEND_OPAQUE";
    }
    return "UNKNOWN";
}

const char* to_string(DeviceState s) noexcept {
    switch (s) {
        case DeviceState::ComputeActive: return "COMPUTE_ACTIVE";
        case DeviceState::ComputeIdle: return "COMPUTE_IDLE";
        case DeviceState::MemoryResident: return "MEMORY_RESIDENT";
        case DeviceState::Reserved: return "RESERVED";
        case DeviceState::Fenced: return "FENCED";
        case DeviceState::Unavailable: return "UNAVAILABLE";
        case DeviceState::Unknown: return "UNKNOWN";
    }
    return "UNKNOWN";
}

const char* to_string(WorkloadState s) noexcept {
    switch (s) {
        case WorkloadState::Running: return "RUNNING";
        case WorkloadState::Queued: return "QUEUED";
        case WorkloadState::WaitingDependency: return "WAITING_DEPENDENCY";
        case WorkloadState::WaitingTransfer: return "WAITING_TRANSFER";
        case WorkloadState::WaitingMemory: return "WAITING_MEMORY";
        case WorkloadState::WaitingRecovery: return "WAITING_RECOVERY";
        case WorkloadState::Retrying: return "RETRYING";
        case WorkloadState::Done: return "DONE";
        case WorkloadState::Failed: return "FAILED";
        case WorkloadState::Cancelled: return "CANCELLED";
        case WorkloadState::Unknown: return "UNKNOWN";
    }
    return "UNKNOWN";
}

const char* to_string(StateCategory c) noexcept {
    switch (c) {
        case StateCategory::UsefulExecution: return "USEFUL_EXECUTION";
        case StateCategory::NonUsefulExecution: return "NON_USEFUL_EXECUTION";
        case StateCategory::NecessaryOverhead: return "NECESSARY_OVERHEAD";
        case StateCategory::TransferActive: return "TRANSFER_ACTIVE";
        case StateCategory::MemoryBound: return "MEMORY_BOUND";
        case StateCategory::WaitingQueue: return "WAITING_QUEUE";
        case StateCategory::WaitingDependency: return "WAITING_DEPENDENCY";
        case StateCategory::WaitingTransfer: return "WAITING_TRANSFER";
        case StateCategory::WaitingMemory: return "WAITING_MEMORY";
        case StateCategory::WaitingRecovery: return "WAITING_RECOVERY";
        case StateCategory::RetryExecution: return "RETRY_EXECUTION";
        case StateCategory::ReservedIdle: return "RESERVED_IDLE";
        case StateCategory::ResidencyIdle: return "RESIDENCY_IDLE";
        case StateCategory::FragmentationStranded: return "FRAGMENTATION_STRANDED";
        case StateCategory::AuthorityFenced: return "AUTHORITY_FENCED";
        case StateCategory::DeviceIdle: return "DEVICE_IDLE";
        case StateCategory::DeviceUnavailable: return "DEVICE_UNAVAILABLE";
        case StateCategory::Unknown: return "UNKNOWN";
    }
    return "UNKNOWN";
}

const char* to_string(BusyReason r) noexcept {
    switch (r) {
        case BusyReason::Useful: return "USEFUL";
        case BusyReason::NecessaryOverhead: return "NECESSARY_OVERHEAD";
        case BusyReason::AvoidableWaste: return "AVOIDABLE_WASTE";
        case BusyReason::Retry: return "RETRY";
        case BusyReason::Recomputation: return "RECOMPUTATION";
        case BusyReason::Recovery: return "RECOVERY";
        case BusyReason::TransferAssist: return "TRANSFER_ASSIST";
        case BusyReason::UnknownBusy: return "UNKNOWN_BUSY";
    }
    return "UNKNOWN";
}

const char* to_string(IdleReason r) noexcept {
    switch (r) {
        case IdleReason::AvailableIdle: return "AVAILABLE_IDLE";
        case IdleReason::ReservedIdle: return "RESERVED_IDLE";
        case IdleReason::ResidencyIdle: return "RESIDENCY_IDLE";
        case IdleReason::DependencyBlocked: return "DEPENDENCY_BLOCKED";
        case IdleReason::AuthorityFenced: return "AUTHORITY_FENCED";
        case IdleReason::FragmentationStranded: return "FRAGMENTATION_STRANDED";
        case IdleReason::CapacityUnfit: return "CAPACITY_UNFIT";
        case IdleReason::WorkloadStarved: return "WORKLOAD_STARVED";
        case IdleReason::RecoveryWait: return "RECOVERY_WAIT";
        case IdleReason::UnknownIdle: return "UNKNOWN_IDLE";
    }
    return "UNKNOWN";
}

const char* to_string(Classification c) noexcept {
    switch (c) {
        case Classification::Useful: return "USEFUL";
        case Classification::NecessaryOverhead: return "NECESSARY_OVERHEAD";
        case Classification::AvoidableWaste: return "AVOIDABLE_WASTE";
        case Classification::AvoidedWork: return "AVOIDED_WORK";
        case Classification::StrandedCapacity: return "STRANDED_CAPACITY";
        case Classification::Unknown: return "UNKNOWN";
    }
    return "UNKNOWN";
}

const char* to_string(ObservationType t) noexcept {
    switch (t) {
        case ObservationType::DeviceSample: return "DEVICE_SAMPLE";
        case ObservationType::WorkloadSample: return "WORKLOAD_SAMPLE";
        case ObservationType::ExecutionBegin: return "EXECUTION_BEGIN";
        case ObservationType::ExecutionEnd: return "EXECUTION_END";
        case ObservationType::TransferBegin: return "TRANSFER_BEGIN";
        case ObservationType::TransferEnd: return "TRANSFER_END";
        case ObservationType::ReservationBegin: return "RESERVATION_BEGIN";
        case ObservationType::ReservationEnd: return "RESERVATION_END";
        case ObservationType::ResidencyBegin: return "RESIDENCY_BEGIN";
        case ObservationType::ResidencyEnd: return "RESIDENCY_END";
        case ObservationType::WaitBegin: return "WAIT_BEGIN";
        case ObservationType::WaitEnd: return "WAIT_END";
        case ObservationType::RecoveryBegin: return "RECOVERY_BEGIN";
        case ObservationType::RecoveryEnd: return "RECOVERY_END";
        case ObservationType::AttemptBegin: return "ATTEMPT_BEGIN";
        case ObservationType::AttemptEnd: return "ATTEMPT_END";
        case ObservationType::WorkerReady: return "WORKER_READY";
        case ObservationType::WorkerLost: return "WORKER_LOST";
        case ObservationType::ResourceAvailable: return "RESOURCE_AVAILABLE";
        case ObservationType::ResourceUnavailable: return "RESOURCE_UNAVAILABLE";
        case ObservationType::FragmentationObserved: return "FRAGMENTATION_OBSERVED";
        case ObservationType::FragmentationCleared: return "FRAGMENTATION_CLEARED";
        case ObservationType::CapacityPublished: return "CAPACITY_PUBLISHED";
        case ObservationType::CapacityInvalidated: return "CAPACITY_INVALIDATED";
        case ObservationType::UsefulWorkSummary: return "USEFUL_WORK_SUMMARY";
        case ObservationType::EfficiencySummary: return "EFFICIENCY_SUMMARY";
        case ObservationType::HealthSample: return "HEALTH_SAMPLE";
        case ObservationType::PowerSample: return "POWER_SAMPLE";
        case ObservationType::MemorySample: return "MEMORY_SAMPLE";
    }
    return "UNKNOWN";
}

const char* to_string(CorrelationKind k) noexcept {
    switch (k) {
        case CorrelationKind::TemporallyAssociated: return "TEMPORALLY_ASSOCIATED";
        case CorrelationKind::PotentialContributor: return "POTENTIAL_CONTRIBUTOR";
        case CorrelationKind::UnknownCause: return "UNKNOWN_CAUSE";
    }
    return "UNKNOWN";
}

const char* to_string(TransferDirection d) noexcept {
    switch (d) {
        case TransferDirection::H2D: return "H2D";
        case TransferDirection::D2H: return "D2H";
        case TransferDirection::P2P: return "P2P";
        case TransferDirection::Network: return "NETWORK";
        case TransferDirection::Storage: return "STORAGE";
        case TransferDirection::HostMemory: return "HOST_MEMORY";
        case TransferDirection::Unknown: return "UNKNOWN";
    }
    return "UNKNOWN";
}

const char* to_string(CapacityRole r) noexcept {
    switch (r) {
        case CapacityRole::Physical: return "PHYSICAL";
        case CapacityRole::Published: return "PUBLISHED";
        case CapacityRole::Available: return "AVAILABLE";
        case CapacityRole::Allocated: return "ALLOCATED";
        case CapacityRole::Reserved: return "RESERVED";
        case CapacityRole::Resident: return "RESIDENT";
        case CapacityRole::FitQualified: return "FIT_QUALIFIED";
        case CapacityRole::Stranded: return "STRANDED";
        case CapacityRole::Blocked: return "BLOCKED";
        case CapacityRole::UnknownCapacity: return "UNKNOWN";
    }
    return "UNKNOWN";
}

const char* to_string(AuthorityLevel a) noexcept {
    switch (a) {
        case AuthorityLevel::Coordinator: return "COORDINATOR";
        case AuthorityLevel::Source: return "SOURCE";
        case AuthorityLevel::Worker: return "WORKER";
        case AuthorityLevel::Device: return "DEVICE";
        case AuthorityLevel::Workload: return "WORKLOAD";
        case AuthorityLevel::Evidence: return "EVIDENCE";
        case AuthorityLevel::None: return "NONE";
    }
    return "UNKNOWN";
}

} // namespace uo
