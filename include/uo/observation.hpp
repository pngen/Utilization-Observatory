#pragma once
// Utilization Observatory : immutable observation record.
#include "uo/ids.hpp"
#include "uo/interval.hpp"
#include "uo/model.hpp"
#include "uo/time.hpp"
#include <cstdint>

namespace uo {

struct Observation {
    ObservationId id;
    ObservationGeneration generation;
    CoordinatorEpoch coordinator_epoch{CoordinatorEpoch(1)};
    ObservationType type{ObservationType::DeviceSample};
    Tick begin{0};
    Tick end{0};
    SourceId source;
    SourceGeneration source_generation;
    DeviceId device;
    DeviceGeneration device_generation;
    HostId host;
    HostGeneration host_generation;
    WorkerId worker;
    WorkerBootId worker_boot;
    WorkloadId workload;
    WorkloadGeneration workload_generation;
    RequestId request;
    AttemptId attempt;
    AttemptGeneration attempt_generation;
    ReservationId reservation;
    ReservationGeneration reservation_generation;
    ResidencyId residency;
    ResidencyGeneration residency_generation;
    AllocationId allocation;
    AllocationGeneration allocation_generation;
    TransferId transfer;
    TransferGeneration transfer_generation;
    RecoveryId recovery;
    RecoveryGeneration recovery_generation;
    FragmentationGeneration fragmentation_generation;
    EvidenceId evidence;
    EvidenceGeneration evidence_generation;
    Provenance provenance{Provenance::Unknown};
    EvidenceLabel evidence_label{EvidenceLabel::Derived};
    SampleSemantics semantics{SampleSemantics::BackendOpaque};
    StateCategory state{StateCategory::Unknown};
    BusyReason busy_reason{BusyReason::UnknownBusy};
    IdleReason idle_reason{IdleReason::UnknownIdle};
    Classification classification{Classification::Unknown};
    CorrelationKind correlation{CorrelationKind::UnknownCause};
    TransferDirection direction{TransferDirection::Unknown};
    DeviceState device_state{DeviceState::Unknown};
    WorkloadState workload_state{WorkloadState::Unknown};
    double ratio{0.0};
    double percent{0.0};
    std::uint64_t bytes{0};
    std::uint64_t capacity_bytes{0};
    std::uint64_t count{0};
    WallTime wall_time{0};
    // The observation span. For interval events this is [begin,end); for
    // point/sample events it is a point at begin with end == begin.
    Interval interval() const noexcept { return Interval{begin, end}; }
};

} // namespace uo
