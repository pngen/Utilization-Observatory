#include "uo/codec.hpp"
#include <cstring>

namespace uo {
namespace {
void put_u8(std::vector<std::uint8_t>& buf, std::uint8_t v) { buf.push_back(v); }
void put_u32(std::vector<std::uint8_t>& buf, std::uint32_t v) {
    for (int i = 0; i < 4; ++i) buf.push_back(static_cast<std::uint8_t>((v >> (8 * i)) & 0xFFu));
}
void put_u64(std::vector<std::uint8_t>& buf, std::uint64_t v) {
    for (int i = 0; i < 8; ++i) buf.push_back(static_cast<std::uint8_t>((v >> (8 * i)) & 0xFFu));
}
void put_i64(std::vector<std::uint8_t>& buf, std::int64_t v) { put_u64(buf, static_cast<std::uint64_t>(v)); }
void put_double(std::vector<std::uint8_t>& buf, double d) {
    std::uint64_t bits = 0;
    std::memcpy(&bits, &d, sizeof(d));
    put_u64(buf, bits);
}
struct Reader {
    const std::uint8_t* p; size_t n; size_t off{0}; bool ok{true};
    std::uint8_t u8() { if (off + 1 > n) { ok = false; return 0; } return p[off++]; }
    std::uint32_t u32() {
        if (off + 4 > n) { ok = false; return 0; }
        std::uint32_t v = 0; for (int i = 0; i < 4; ++i) v |= static_cast<std::uint32_t>(p[off + i]) << (8 * i);
        off += 4; return v;
    }
    std::uint64_t u64() {
        if (off + 8 > n) { ok = false; return 0; }
        std::uint64_t v = 0; for (int i = 0; i < 8; ++i) v |= static_cast<std::uint64_t>(p[off + i]) << (8 * i);
        off += 8; return v;
    }
    std::int64_t i64() { return static_cast<std::int64_t>(u64()); }
    double dbl() { std::uint64_t bits = u64(); double d = 0; std::memcpy(&d, &bits, sizeof(d)); return d; }
};
} // namespace

void encode_observation(std::vector<std::uint8_t>& buf, const Observation& o) {
    put_u64(buf, o.id.value()); put_u64(buf, o.generation.value()); put_u64(buf, o.coordinator_epoch.value());
    put_u8(buf, static_cast<std::uint8_t>(o.type)); put_u64(buf, o.begin); put_u64(buf, o.end);
    put_u64(buf, o.source.value()); put_u64(buf, o.source_generation.value()); put_u64(buf, o.device.value());
    put_u64(buf, o.device_generation.value()); put_u64(buf, o.host.value()); put_u64(buf, o.host_generation.value());
    put_u64(buf, o.worker.value()); put_u64(buf, o.worker_boot.value()); put_u64(buf, o.workload.value());
    put_u64(buf, o.workload_generation.value()); put_u64(buf, o.request.value()); put_u64(buf, o.attempt.value());
    put_u64(buf, o.attempt_generation.value()); put_u64(buf, o.reservation.value()); put_u64(buf, o.reservation_generation.value());
    put_u64(buf, o.residency.value()); put_u64(buf, o.residency_generation.value()); put_u64(buf, o.allocation.value());
    put_u64(buf, o.allocation_generation.value()); put_u64(buf, o.transfer.value()); put_u64(buf, o.transfer_generation.value());
    put_u64(buf, o.recovery.value()); put_u64(buf, o.recovery_generation.value()); put_u64(buf, o.fragmentation_generation.value());
    put_u64(buf, o.evidence.value()); put_u64(buf, o.evidence_generation.value());
    put_u8(buf, static_cast<std::uint8_t>(o.provenance)); put_u8(buf, static_cast<std::uint8_t>(o.evidence_label));
    put_u8(buf, static_cast<std::uint8_t>(o.semantics)); put_u8(buf, static_cast<std::uint8_t>(o.state));
    put_u8(buf, static_cast<std::uint8_t>(o.busy_reason)); put_u8(buf, static_cast<std::uint8_t>(o.idle_reason));
    put_u8(buf, static_cast<std::uint8_t>(o.classification)); put_u8(buf, static_cast<std::uint8_t>(o.correlation));
    put_u8(buf, static_cast<std::uint8_t>(o.direction)); put_u8(buf, static_cast<std::uint8_t>(o.device_state));
    put_u8(buf, static_cast<std::uint8_t>(o.workload_state));
    put_double(buf, o.ratio); put_double(buf, o.percent); put_u64(buf, o.bytes); put_u64(buf, o.capacity_bytes);
    put_u64(buf, o.count); put_i64(buf, o.wall_time);
}

bool decode_observation(const std::uint8_t* data, std::size_t n, std::size_t& off_out, Observation& o) {
    Reader r{data, n};
    o.id = ObservationId(r.u64()); o.generation = ObservationGeneration(r.u64()); o.coordinator_epoch = CoordinatorEpoch(r.u64());
    o.type = static_cast<ObservationType>(r.u8()); o.begin = r.u64(); o.end = r.u64();
    o.source = SourceId(r.u64()); o.source_generation = SourceGeneration(r.u64()); o.device = DeviceId(r.u64());
    o.device_generation = DeviceGeneration(r.u64()); o.host = HostId(r.u64()); o.host_generation = HostGeneration(r.u64());
    o.worker = WorkerId(r.u64()); o.worker_boot = WorkerBootId(r.u64()); o.workload = WorkloadId(r.u64());
    o.workload_generation = WorkloadGeneration(r.u64()); o.request = RequestId(r.u64()); o.attempt = AttemptId(r.u64());
    o.attempt_generation = AttemptGeneration(r.u64()); o.reservation = ReservationId(r.u64());
    o.reservation_generation = ReservationGeneration(r.u64()); o.residency = ResidencyId(r.u64());
    o.residency_generation = ResidencyGeneration(r.u64()); o.allocation = AllocationId(r.u64());
    o.allocation_generation = AllocationGeneration(r.u64()); o.transfer = TransferId(r.u64());
    o.transfer_generation = TransferGeneration(r.u64()); o.recovery = RecoveryId(r.u64());
    o.recovery_generation = RecoveryGeneration(r.u64()); o.fragmentation_generation = FragmentationGeneration(r.u64());
    o.evidence = EvidenceId(r.u64()); o.evidence_generation = EvidenceGeneration(r.u64());
    o.provenance = static_cast<Provenance>(r.u8()); o.evidence_label = static_cast<EvidenceLabel>(r.u8());
    o.semantics = static_cast<SampleSemantics>(r.u8()); o.state = static_cast<StateCategory>(r.u8());
    o.busy_reason = static_cast<BusyReason>(r.u8()); o.idle_reason = static_cast<IdleReason>(r.u8());
    o.classification = static_cast<Classification>(r.u8()); o.correlation = static_cast<CorrelationKind>(r.u8());
    o.direction = static_cast<TransferDirection>(r.u8()); o.device_state = static_cast<DeviceState>(r.u8());
    o.workload_state = static_cast<WorkloadState>(r.u8());
    o.ratio = r.dbl(); o.percent = r.dbl(); o.bytes = r.u64(); o.capacity_bytes = r.u64(); o.count = r.u64(); o.wall_time = r.i64();
    if (!r.ok || r.off != n) return false;
    off_out = r.off;
    return true;
}

} // namespace uo
