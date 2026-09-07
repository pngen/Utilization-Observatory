#pragma once
// Utilization Observatory : strongly typed identities.
// Distinct tag types guard against interchanging raw integers across authority
// domains. Each identity is a thin wrapper around an opaque value.
// Copyright 2026 Summon Software Labs. Apache-2.0.

#include <cstdint>
#include <functional>

namespace uo {

template <typename Tag>
struct BasicId {
    using value_type = std::uint64_t;
    using tag_type = Tag;

    constexpr BasicId() noexcept = default;
    explicit constexpr BasicId(value_type v) noexcept : value_(v) {}

    constexpr value_type value() const noexcept { return value_; }
    constexpr bool valid() const noexcept { return value_ != 0; }
    explicit constexpr operator bool() const noexcept { return valid(); }

    friend constexpr bool operator==(BasicId a, BasicId b) noexcept { return a.value_ == b.value_; }
    friend constexpr bool operator!=(BasicId a, BasicId b) noexcept { return a.value_ != b.value_; }
    friend constexpr bool operator<(BasicId a, BasicId b) noexcept { return a.value_ < b.value_; }
    friend constexpr bool operator<=(BasicId a, BasicId b) noexcept { return a.value_ <= b.value_; }
    friend constexpr bool operator>(BasicId a, BasicId b) noexcept { return a.value_ > b.value_; }
    friend constexpr bool operator>=(BasicId a, BasicId b) noexcept { return a.value_ >= b.value_; }

    constexpr BasicId& operator++() noexcept { ++value_; return *this; }
    constexpr BasicId operator++(int) noexcept { BasicId t = *this; ++value_; return t; }

private:
    value_type value_{0};
};

struct CoordinatorEpochTag      {};
struct ObservatoryIdTag         {};
struct ObservatoryGenerationTag {};
struct SourceIdTag              {};
struct SourceGenerationTag      {};
struct WorkerIdTag              {};
struct WorkerBootIdTag          {};
struct HostIdTag                {};
struct HostGenerationTag        {};
struct DeviceIdTag              {};
struct DeviceGenerationTag      {};
struct ResourceIdTag            {};
struct ResourceGenerationTag    {};
struct WorkloadIdTag            {};
struct WorkloadGenerationTag    {};
struct RequestIdTag             {};
struct AttemptIdTag             {};
struct AttemptGenerationTag     {};
struct ReservationIdTag         {};
struct ReservationGenerationTag {};
struct AllocationIdTag          {};
struct AllocationGenerationTag  {};
struct ResidencyIdTag           {};
struct ResidencyGenerationTag   {};
struct TransferIdTag            {};
struct TransferGenerationTag    {};
struct RecoveryIdTag            {};
struct RecoveryGenerationTag    {};
struct FragmentationGenerationTag {};
struct EvidenceIdTag            {};
struct EvidenceGenerationTag    {};
struct ObservationIdTag         {};
struct ObservationGenerationTag {};
struct TimelineIdTag            {};
struct SnapshotIdTag            {};

using CoordinatorEpoch       = BasicId<CoordinatorEpochTag>;
using ObservatoryId          = BasicId<ObservatoryIdTag>;
using ObservatoryGeneration  = BasicId<ObservatoryGenerationTag>;
using SourceId               = BasicId<SourceIdTag>;
using SourceGeneration       = BasicId<SourceGenerationTag>;
using WorkerId               = BasicId<WorkerIdTag>;
using WorkerBootId           = BasicId<WorkerBootIdTag>;
using HostId                 = BasicId<HostIdTag>;
using HostGeneration         = BasicId<HostGenerationTag>;
using DeviceId               = BasicId<DeviceIdTag>;
using DeviceGeneration       = BasicId<DeviceGenerationTag>;
using ResourceId             = BasicId<ResourceIdTag>;
using ResourceGeneration     = BasicId<ResourceGenerationTag>;
using WorkloadId             = BasicId<WorkloadIdTag>;
using WorkloadGeneration     = BasicId<WorkloadGenerationTag>;
using RequestId              = BasicId<RequestIdTag>;
using AttemptId              = BasicId<AttemptIdTag>;
using AttemptGeneration      = BasicId<AttemptGenerationTag>;
using ReservationId          = BasicId<ReservationIdTag>;
using ReservationGeneration  = BasicId<ReservationGenerationTag>;
using AllocationId           = BasicId<AllocationIdTag>;
using AllocationGeneration   = BasicId<AllocationGenerationTag>;
using ResidencyId            = BasicId<ResidencyIdTag>;
using ResidencyGeneration    = BasicId<ResidencyGenerationTag>;
using TransferId             = BasicId<TransferIdTag>;
using TransferGeneration     = BasicId<TransferGenerationTag>;
using RecoveryId             = BasicId<RecoveryIdTag>;
using RecoveryGeneration     = BasicId<RecoveryGenerationTag>;
using FragmentationGeneration = BasicId<FragmentationGenerationTag>;
using EvidenceId             = BasicId<EvidenceIdTag>;
using EvidenceGeneration     = BasicId<EvidenceGenerationTag>;
using ObservationId          = BasicId<ObservationIdTag>;
using ObservationGeneration  = BasicId<ObservationGenerationTag>;
using TimelineId             = BasicId<TimelineIdTag>;
using SnapshotId             = BasicId<SnapshotIdTag>;

} // namespace uo

namespace std {
template <typename Tag>
struct hash<uo::BasicId<Tag>> {
    size_t operator()(const uo::BasicId<Tag>& id) const noexcept {
        return std::hash<typename uo::BasicId<Tag>::value_type>()(id.value());
    }
};
} // namespace std
