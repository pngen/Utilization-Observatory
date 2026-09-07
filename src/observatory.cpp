#include "uo/observatory.hpp"
#include "uo/arithmetic.hpp"
#include "uo/crc.hpp"
#include "uo/digest.hpp"
#include "uo/interval.hpp"
#include "uo/model.hpp"
#include "uo/observation.hpp"
#include "uo/source.hpp"
#include "uo/status.hpp"
#include "uo/summary.hpp"
#include "uo/time.hpp"
#include "uo/view.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <deque>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace uo {
namespace {

// Interval key couples a BEGIN to its matching END. It identifies the event kind
// (Execution/Transfer/Reservation/Residency/Wait/Recovery/Attempt) plus the
// semantic scope, but deliberately excludes the state category so BEGIN and END
// always match regardless of how the category is labeled on either side.
struct IntervalKey {
    SourceId src;
    DeviceId dev;
    ObservationType type{ObservationType::ExecutionBegin};
    WorkloadId wl;
    AttemptId att;
    ReservationId res;
    ResidencyId resi;
    TransferId tr;
    RecoveryId rec;
    AllocationId alloc;

    bool operator==(const IntervalKey& o) const noexcept {
        return src == o.src && dev == o.dev && type == o.type && wl == o.wl && att == o.att &&
               res == o.res && resi == o.resi && tr == o.tr && rec == o.rec && alloc == o.alloc;
    }
};

struct IntervalKeyHash {
    size_t operator()(const IntervalKey& k) const noexcept {
        size_t h = static_cast<size_t>(0x9e3779b9u);
        h ^= k.src.value() + 0x9e3779b9u + (h << 6) + (h >> 2);
        h ^= k.dev.value() + 0x9e3779b9u + (h << 6) + (h >> 2);
        h ^= static_cast<size_t>(k.type) + 0x9e3779b9u + (h << 6) + (h >> 2);
        h ^= k.wl.value() + 0x9e3779b9u + (h << 6) + (h >> 2);
        h ^= k.att.value() + 0x9e3779b9u + (h << 6) + (h >> 2);
        h ^= k.res.value() + 0x9e3779b9u + (h << 6) + (h >> 2);
        h ^= k.resi.value() + 0x9e3779b9u + (h << 6) + (h >> 2);
        h ^= k.tr.value() + 0x9e3779b9u + (h << 6) + (h >> 2);
        h ^= k.rec.value() + 0x9e3779b9u + (h << 6) + (h >> 2);
        h ^= k.alloc.value() + 0x9e3779b9u + (h << 6) + (h >> 2);
        return h;
    }
};

struct EventSpan {
    Interval span;
    StateCategory cat{StateCategory::Unknown};
    SourceId src;
    SourceGeneration srcgen;
    WorkloadId wl;
    WorkloadGeneration wlgen;
    DeviceGeneration devgen;
    Provenance prov{Provenance::Unknown};
    Freshness fresh{Freshness::Unknown};
    ObservationId obs;
    double ratio{0.0};
    std::uint64_t bytes{0};
};

struct OpenEvent {
    IntervalKey key;
    Tick begin{0};
    StateCategory cat{StateCategory::Unknown};
    ObservationId obs;
    SourceId src;
    SourceGeneration srcgen;
    WorkloadId wl;
    WorkloadGeneration wlgen;
    DeviceGeneration devgen;
    Provenance prov{Provenance::Unknown};
    Freshness fresh{Freshness::Unknown};
    double ratio{0.0};
    std::uint64_t bytes{0};
};

struct SamplePoint {
    Tick at{0};
    double percent{0.0};
    bool defined{false};
    std::uint64_t bytes{0};
    std::uint64_t capacity_bytes{0};
    WallTime wall{0};
    Provenance prov{Provenance::Unknown};
    SourceId src;
};

struct UsefulBand {
    Interval span;
    double useful_ratio{0.0};
    SourceId src;
    SourceGeneration srcgen;
    Provenance prov{Provenance::Unknown};
    Freshness fresh{Freshness::Unknown};
};

struct SourceState {
    SourceInfo info;
    bool connected{false};
    std::uint64_t accepted{0};
    std::uint64_t rejected{0};
    std::uint64_t duplicates{0};
    std::uint64_t stale{0};
};

struct DeviceAccum {
    DeviceId id;
    SourceId authoritative;
    SourceGeneration authoritative_gen;
    BackendCapabilities caps;
    CapacityState cap;
    DeviceState physical{DeviceState::Unknown};
    double last_vendor_percent{0.0};
    bool vendor_defined{false};
    Tick last_sample_tick{0};
    WallTime last_wall{0};
    std::vector<EventSpan> events;
    std::unordered_map<IntervalKey, OpenEvent, IntervalKeyHash> open;
    std::vector<SamplePoint> samples;
    std::vector<UsefulBand> useful_bands;
};

bool is_begin_pair(ObservationType t) {
    switch (t) {
        case ObservationType::ExecutionBegin:
        case ObservationType::TransferBegin:
        case ObservationType::ReservationBegin:
        case ObservationType::ResidencyBegin:
        case ObservationType::WaitBegin:
        case ObservationType::RecoveryBegin:
        case ObservationType::AttemptBegin:
            return true;
        default:
            return false;
    }
}
bool is_end_pair(ObservationType t) {
    switch (t) {
        case ObservationType::ExecutionEnd:
        case ObservationType::TransferEnd:
        case ObservationType::ReservationEnd:
        case ObservationType::ResidencyEnd:
        case ObservationType::WaitEnd:
        case ObservationType::RecoveryEnd:
        case ObservationType::AttemptEnd:
            return true;
        default:
            return false;
    }
}

ObservationType pair_type(ObservationType t) {
    switch (t) {
        case ObservationType::ExecutionEnd: return ObservationType::ExecutionBegin;
        case ObservationType::TransferEnd: return ObservationType::TransferBegin;
        case ObservationType::ReservationEnd: return ObservationType::ReservationBegin;
        case ObservationType::ResidencyEnd: return ObservationType::ResidencyBegin;
        case ObservationType::WaitEnd: return ObservationType::WaitBegin;
        case ObservationType::RecoveryEnd: return ObservationType::RecoveryBegin;
        case ObservationType::AttemptEnd: return ObservationType::AttemptBegin;
        default: return t;
    }
}

StateCategory default_category(ObservationType t, Classification c, StateCategory state) {
    if (state != StateCategory::Unknown) return state;
    switch (t) {
        case ObservationType::ExecutionBegin:
        case ObservationType::ExecutionEnd:
            if (c == Classification::Useful) return StateCategory::UsefulExecution;
            if (c == Classification::NecessaryOverhead) return StateCategory::NecessaryOverhead;
            if (c == Classification::AvoidableWaste) return StateCategory::NonUsefulExecution;
            if (c == Classification::AvoidedWork) return StateCategory::DeviceIdle;
            return StateCategory::Unknown;
        case ObservationType::TransferBegin:
        case ObservationType::TransferEnd:
            return StateCategory::TransferActive;
        case ObservationType::ReservationBegin:
        case ObservationType::ReservationEnd:
            return StateCategory::ReservedIdle;
        case ObservationType::ResidencyBegin:
        case ObservationType::ResidencyEnd:
            return StateCategory::ResidencyIdle;
        case ObservationType::WaitBegin:
        case ObservationType::WaitEnd:
            return StateCategory::WaitingQueue;
        case ObservationType::RecoveryBegin:
        case ObservationType::RecoveryEnd:
            return StateCategory::WaitingRecovery;
        case ObservationType::AttemptBegin:
        case ObservationType::AttemptEnd:
            return StateCategory::RetryExecution;
        default:
            return StateCategory::Unknown;
    }
}

bool is_busy_category(StateCategory c) {
    switch (c) {
        case StateCategory::UsefulExecution:
        case StateCategory::NonUsefulExecution:
        case StateCategory::NecessaryOverhead:
        case StateCategory::RetryExecution:
        case StateCategory::MemoryBound:
            return true;
        default:
            return false;
    }
}

bool is_wait_category(StateCategory c) {
    return c == StateCategory::WaitingQueue || c == StateCategory::WaitingDependency ||
           c == StateCategory::WaitingTransfer || c == StateCategory::WaitingMemory ||
           c == StateCategory::WaitingRecovery;
}

std::vector<Interval> merge_intervals(std::vector<Interval> ivs) {
    std::sort(ivs.begin(), ivs.end(), [](const Interval& a, const Interval& b) {
        if (a.begin != b.begin) return a.begin < b.begin;
        return a.end < b.end;
    });
    std::vector<Interval> out;
    for (const auto& iv : ivs) {
        if (iv.end <= iv.begin) continue;
        if (out.empty() || iv.begin >= out.back().end) out.push_back(iv);
        else if (iv.end > out.back().end) out.back().end = iv.end;
    }
    return out;
}

Tick union_duration(const std::vector<Interval>& ivs) {
    Tick total = 0;
    for (const auto& iv : merge_intervals(ivs)) total += iv.duration();
    return total;
}

Tick intersect_union_duration(std::vector<Interval> a, std::vector<Interval> b) {
    auto ma = merge_intervals(std::move(a));
    auto mb = merge_intervals(std::move(b));
    Tick total = 0;
    size_t i = 0, j = 0;
    while (i < ma.size() && j < mb.size()) {
        Tick b0 = std::max(ma[i].begin, mb[j].begin);
        Tick b1 = std::min(ma[i].end, mb[j].end);
        if (b1 > b0) total += (b1 - b0);
        if (ma[i].end < mb[j].end) ++i; else ++j;
    }
    return total;
}

} // namespace

struct UtilizationObservatory::Impl {
    Config cfg;
    std::shared_ptr<MonotonicClock> clock;
    CoordinatorEpoch epoch;
    ObservatoryGeneration generation;

    mutable std::shared_mutex mtx;
    std::unordered_map<SourceId, SourceState> sources;
    std::unordered_map<DeviceId, DeviceAccum> devices;
    std::unordered_map<WorkloadId, WorkloadGeneration> workload_gens;
    std::unordered_map<EvidenceId, EvidenceGeneration> evidence_gens;
    std::unordered_map<DeviceId, DeviceGeneration> device_gens;

    std::deque<Observation> history;
    std::unordered_set<ObservationId> dedup;
    std::deque<ObservationId> dedup_order;
    bool historical_mode{false};   // set during load()/replay(); relaxes epoch/authority gating

    std::atomic<std::uint64_t> a_accepted{0};
    std::atomic<std::uint64_t> a_rejected{0};
    std::atomic<std::uint64_t> a_duplicates{0};
    std::atomic<std::uint64_t> a_stale{0};
    std::atomic<std::uint64_t> a_disconnects{0};
    std::atomic<std::uint64_t> a_reconnects{0};
    std::atomic<std::uint64_t> a_persist_errors{0};
    std::atomic<std::uint64_t> a_backend_errors{0};
    std::atomic<std::uint64_t> a_dropped{0};

    explicit Impl(Config c, std::shared_ptr<MonotonicClock> clk)
        : cfg(std::move(c)), clock(std::move(clk)),
          epoch(cfg.epoch), generation(cfg.generation) {}

    Tick now() const { return clock ? clock->now_tick() : Tick{0}; }

    Freshness eval_freshness(const SourceState& s, Tick now) const {
        if (!s.connected) return Freshness::RevalidationRequired;
        Tick last = s.info.last_observation;
        if (last == 0) return Freshness::Unknown;
        if (now >= last && (now - last) > cfg.expire_after_ns) return Freshness::Expired;
        if (now >= last && (now - last) > cfg.stale_after_ns) return Freshness::Stale;
        return Freshness::Current;
    }

    SourceHealth eval_health(const SourceState& s, Tick now) const {
        switch (eval_freshness(s, now)) {
            case Freshness::Current: return SourceHealth::Healthy;
            case Freshness::Stale:
            case Freshness::Expired: return SourceHealth::Stale;
            case Freshness::RevalidationRequired: return SourceHealth::Disconnected;
            case Freshness::Unknown:
            case Freshness::Historical: return SourceHealth::Unknown;
        }
        return SourceHealth::Unknown;
    }

    void remember_obs_id(ObservationId id) {
        if (dedup.size() >= cfg.dedup_capacity && !dedup_order.empty()) {
            ObservationId oldest = dedup_order.front();
            dedup_order.pop_front();
            dedup.erase(oldest);
        }
        dedup.insert(id);
        dedup_order.push_back(id);
    }
    bool seen_obs_id(ObservationId id) const { return dedup.count(id) != 0; }
};

UtilizationObservatory::UtilizationObservatory(Config cfg)
    : UtilizationObservatory(std::move(cfg), std::make_shared<SteadyClock>()) {}

UtilizationObservatory::UtilizationObservatory(Config cfg, std::shared_ptr<MonotonicClock> clock)
    : impl_(std::make_unique<Impl>(std::move(cfg), std::move(clock))), cfg_(impl_->cfg) {}

UtilizationObservatory::~UtilizationObservatory() = default;
UtilizationObservatory::UtilizationObservatory(UtilizationObservatory&&) noexcept = default;
UtilizationObservatory& UtilizationObservatory::operator=(UtilizationObservatory&&) noexcept = default;

CoordinatorEpoch UtilizationObservatory::epoch() const noexcept { return impl_->epoch; }

void UtilizationObservatory::advance_epoch() {
    std::unique_lock<std::shared_mutex> lock(impl_->mtx);
    impl_->epoch = CoordinatorEpoch(impl_->epoch.value() + 1);
    impl_->cfg.epoch = impl_->epoch;
}

Status UtilizationObservatory::register_source(SourceInfo& info) {
    std::unique_lock<std::shared_mutex> lock(impl_->mtx);
    SourceState st;
    st.info = info;
    st.connected = true;
    st.info.freshness = Freshness::Current;
    st.info.health = SourceHealth::Healthy;
    impl_->sources[info.id] = st;
    return Status::success();
}

Status UtilizationObservatory::mark_source_connected(SourceId id, Tick t) {
    std::unique_lock<std::shared_mutex> lock(impl_->mtx);
    auto it = impl_->sources.find(id);
    if (it == impl_->sources.end()) return Status(StatusCode::InvalidInput, "unknown source");
    if (!it->second.connected) impl_->a_reconnects.fetch_add(1, std::memory_order_relaxed);
    it->second.connected = true;
    it->second.info.health = SourceHealth::Healthy;
    it->second.info.freshness = Freshness::Current;
    it->second.info.last_observation = t;
    return Status::success();
}

Status UtilizationObservatory::mark_source_disconnected(SourceId id, Tick t) {
    std::unique_lock<std::shared_mutex> lock(impl_->mtx);
    auto it = impl_->sources.find(id);
    if (it == impl_->sources.end()) return Status(StatusCode::InvalidInput, "unknown source");
    if (it->second.connected) impl_->a_disconnects.fetch_add(1, std::memory_order_relaxed);
    it->second.connected = false;
    it->second.info.health = SourceHealth::Disconnected;
    it->second.info.freshness = Freshness::RevalidationRequired;
    it->second.info.last_observation = t;
    return Status::success();
}

Status UtilizationObservatory::ingest(const Observation& obs) {
    std::unique_lock<std::shared_mutex> lock(impl_->mtx);
    return ingest_locked(obs);
}

Status UtilizationObservatory::ingest_locked(const Observation& obs) {
    auto& I = *impl_;

    if (!I.historical_mode && obs.coordinator_epoch != I.epoch) {
        I.a_stale.fetch_add(1, std::memory_order_relaxed);
        return Status(StatusCode::StaleAuthority, "old coordinator epoch");
    }

    auto srcIt = I.sources.find(obs.source);
    if (srcIt == I.sources.end()) {
        SourceState st;
        st.info.id = obs.source;
        st.info.generation = obs.source_generation;
        st.info.worker = obs.worker;
        st.info.boot = obs.worker_boot;
        st.info.host = obs.host;
        st.info.host_generation = obs.host_generation;
        st.connected = true;
        I.sources[obs.source] = st;
        srcIt = I.sources.find(obs.source);
    }
    SourceState& src = srcIt->second;
    if (I.historical_mode) {
        // Committed history replay: adopt the observation's authority as-is.
        src.info.generation = obs.source_generation;
        src.info.boot = obs.worker_boot;
        src.connected = true;
    } else {
        bool authority_ok = (obs.source_generation == src.info.generation && obs.worker_boot == src.info.boot);
        if (!authority_ok && !src.connected) {
            // A disconnected source (e.g. worker death / coordinator restart) may
            // re-establish authority with a fresh boot/generation.
            src.info.generation = obs.source_generation;
            src.info.boot = obs.worker_boot;
            src.connected = true;
            src.info.freshness = Freshness::Current;
            src.info.health = SourceHealth::Healthy;
            src.info.last_observation = obs.begin;
            authority_ok = true;
            I.a_reconnects.fetch_add(1, std::memory_order_relaxed);
        }
        if (!authority_ok) {
            ++src.stale;
            I.a_stale.fetch_add(1, std::memory_order_relaxed);
            return Status(StatusCode::StaleAuthority, "stale source generation or boot");
        }
        if (!src.connected) {
            ++src.rejected;
            I.a_rejected.fetch_add(1, std::memory_order_relaxed);
            return Status(StatusCode::SourceDisconnected, "source disconnected");
        }
    }

    if (obs.device.valid()) {
        auto dg = I.device_gens.find(obs.device);
        if (dg == I.device_gens.end()) I.device_gens[obs.device] = obs.device_generation;
        else if (obs.device_generation.value() < dg->second.value()) {
            I.a_stale.fetch_add(1, std::memory_order_relaxed);
            return Status(StatusCode::StaleEvidence, "stale device generation");
        } else if (obs.device_generation.value() > dg->second.value()) {
            I.device_gens[obs.device] = obs.device_generation;
        }
    }
    if (obs.workload.valid()) {
        auto wg = I.workload_gens.find(obs.workload);
        if (wg == I.workload_gens.end()) I.workload_gens[obs.workload] = obs.workload_generation;
        else if (obs.workload_generation.value() < wg->second.value()) {
            I.a_stale.fetch_add(1, std::memory_order_relaxed);
            return Status(StatusCode::StaleEvidence, "stale workload generation");
        } else if (obs.workload_generation.value() > wg->second.value()) {
            I.workload_gens[obs.workload] = obs.workload_generation;
        }
    }
    if (obs.evidence.valid()) {
        auto eg = I.evidence_gens.find(obs.evidence);
        if (eg == I.evidence_gens.end()) I.evidence_gens[obs.evidence] = obs.evidence_generation;
        else if (obs.evidence_generation.value() < eg->second.value()) {
            I.a_stale.fetch_add(1, std::memory_order_relaxed);
            return Status(StatusCode::StaleEvidence, "stale evidence generation");
        } else if (obs.evidence_generation.value() > eg->second.value()) {
            I.evidence_gens[obs.evidence] = obs.evidence_generation;
        }
    }

    if (I.seen_obs_id(obs.id)) {
        ++src.duplicates;
        I.a_duplicates.fetch_add(1, std::memory_order_relaxed);
        return Status(StatusCode::DuplicateObservation, "duplicate observation id");
    }
    I.remember_obs_id(obs.id);

    if (obs.end < obs.begin || std::isnan(obs.ratio) || std::isinf(obs.ratio) ||
        obs.ratio < 0.0 || obs.ratio > 1.0 || std::isnan(obs.percent) ||
        std::isinf(obs.percent) || obs.percent < 0.0 || obs.percent > 100.0) {
        I.a_rejected.fetch_add(1, std::memory_order_relaxed);
        return Status(StatusCode::InvalidInput, "malformed observation");
    }

    src.info.last_observation = std::max(src.info.last_observation, obs.begin);
    if (obs.wall_time != 0) src.info.last_wall = obs.wall_time;
    if (src.info.backend.empty()) src.info.backend = "unknown";
    src.info.freshness = Freshness::Current;
    src.info.health = SourceHealth::Healthy;

    DeviceAccum* dev = nullptr;
    if (obs.device.valid()) {
        auto dit = I.devices.find(obs.device);
        if (dit == I.devices.end()) {
            DeviceAccum d;
            d.id = obs.device;
            d.authoritative = obs.source;
            d.authoritative_gen = obs.source_generation;
            d.caps = src.info.capabilities;
            I.devices[obs.device] = d;
            dit = I.devices.find(obs.device);
        }
        dev = &dit->second;
        if (!dev->authoritative.valid() || obs.source == dev->authoritative) {
            dev->authoritative = obs.source;
            dev->authoritative_gen = obs.source_generation;
            dev->caps = src.info.capabilities;
        }
        if (obs.device_state != DeviceState::Unknown) dev->physical = obs.device_state;
    }

    if (is_begin_pair(obs.type)) {
        if (dev) {
            IntervalKey key{obs.source, obs.device, obs.type, obs.workload, obs.attempt,
                            obs.reservation, obs.residency, obs.transfer, obs.recovery, obs.allocation};
            OpenEvent oe{key, obs.begin, obs.state, obs.id, obs.source, obs.source_generation,
                         obs.workload, obs.workload_generation, obs.device_generation,
                         obs.provenance, Freshness::Current, obs.ratio, obs.bytes};
            dev->open[key] = oe;
        }
    } else if (is_end_pair(obs.type)) {
        if (dev) {
            ObservationType pt = pair_type(obs.type);
            IntervalKey key{obs.source, obs.device, pt, obs.workload, obs.attempt,
                            obs.reservation, obs.residency, obs.transfer, obs.recovery, obs.allocation};
            auto it = dev->open.find(key);
            if (it != dev->open.end()) {
                OpenEvent& oe = it->second;
                Tick end = obs.begin;
                if (obs.end > obs.begin) end = obs.end;
                if (end < oe.begin) end = oe.begin;
                StateCategory cat = default_category(obs.type, obs.classification, obs.state);
                if (cat == StateCategory::Unknown && oe.cat != StateCategory::Unknown)
                    cat = oe.cat;
                EventSpan ev{Interval{oe.begin, end}, cat, oe.src, oe.srcgen, oe.wl, oe.wlgen,
                             oe.devgen, oe.prov, Freshness::Current, oe.obs, oe.ratio, oe.bytes};
                dev->events.push_back(ev);
                dev->open.erase(it);
            }
        }
    } else {
        switch (obs.type) {
            case ObservationType::DeviceSample:
            case ObservationType::WorkloadSample: {
                if (dev) {
                    if (dev->samples.size() > 1024) dev->samples.erase(dev->samples.begin());
                    dev->samples.push_back(SamplePoint{obs.begin, obs.percent, true, obs.bytes,
                        obs.capacity_bytes, obs.wall_time, obs.provenance, obs.source});
                    if (obs.type == ObservationType::DeviceSample) {
                        dev->last_vendor_percent = obs.percent;
                        dev->vendor_defined = true;
                        dev->last_sample_tick = obs.begin;
                    }
                }
                break;
            }
            case ObservationType::MemorySample: {
                if (dev && obs.capacity_bytes > 0) {
                    dev->cap.physical_bytes = obs.capacity_bytes;
                    dev->cap.available_bytes = obs.bytes;
                }
                break;
            }
            case ObservationType::UsefulWorkSummary:
            case ObservationType::EfficiencySummary: {
                if (dev && obs.end > obs.begin) {
                    UsefulBand ub{Interval{obs.begin, obs.end}, obs.ratio, obs.source,
                                  obs.source_generation, obs.provenance, Freshness::Current};
                    dev->useful_bands.push_back(ub);
                }
                break;
            }
            case ObservationType::CapacityPublished: {
                if (dev) {
                    dev->cap.published_bytes = obs.capacity_bytes;
                    dev->cap.physical_bytes = std::max(dev->cap.physical_bytes, obs.capacity_bytes);
                }
                break;
            }
            case ObservationType::CapacityInvalidated: {
                if (dev) dev->cap.published_bytes = 0;
                break;
            }
            case ObservationType::FragmentationObserved: {
                if (dev) {
                    dev->cap.stranded_bytes = obs.bytes;
                    dev->cap.fit_qualified_bytes = obs.capacity_bytes;
                }
                break;
            }
            case ObservationType::FragmentationCleared: {
                if (dev) dev->cap.stranded_bytes = 0;
                break;
            }
            case ObservationType::HealthSample:
            case ObservationType::PowerSample: {
                break;
            }
            case ObservationType::WorkerReady:
            case ObservationType::WorkerLost:
            case ObservationType::ResourceAvailable:
            case ObservationType::ResourceUnavailable: {
                break;
            }
            default: {
                // Execution* other than begin/end is not expected; ignore.
                break;
            }
        }
    }

    I.history.push_back(obs);
    if (I.history.size() > I.cfg.max_observations) {
        I.history.pop_front();
        I.a_dropped.fetch_add(1, std::memory_order_relaxed);
    }

    ++src.accepted;
    I.a_accepted.fetch_add(1, std::memory_order_relaxed);
    return Status::success();
}

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
void put_bool(std::vector<std::uint8_t>& buf, bool b) { buf.push_back(b ? 1 : 0); }

void encode_observation(std::vector<std::uint8_t>& buf, const Observation& o) {
    put_u64(buf, o.id.value());
    put_u64(buf, o.generation.value());
    put_u64(buf, o.coordinator_epoch.value());
    put_u8(buf, static_cast<std::uint8_t>(o.type));
    put_u64(buf, o.begin);
    put_u64(buf, o.end);
    put_u64(buf, o.source.value());
    put_u64(buf, o.source_generation.value());
    put_u64(buf, o.device.value());
    put_u64(buf, o.device_generation.value());
    put_u64(buf, o.host.value());
    put_u64(buf, o.host_generation.value());
    put_u64(buf, o.worker.value());
    put_u64(buf, o.worker_boot.value());
    put_u64(buf, o.workload.value());
    put_u64(buf, o.workload_generation.value());
    put_u64(buf, o.request.value());
    put_u64(buf, o.attempt.value());
    put_u64(buf, o.attempt_generation.value());
    put_u64(buf, o.reservation.value());
    put_u64(buf, o.reservation_generation.value());
    put_u64(buf, o.residency.value());
    put_u64(buf, o.residency_generation.value());
    put_u64(buf, o.allocation.value());
    put_u64(buf, o.allocation_generation.value());
    put_u64(buf, o.transfer.value());
    put_u64(buf, o.transfer_generation.value());
    put_u64(buf, o.recovery.value());
    put_u64(buf, o.recovery_generation.value());
    put_u64(buf, o.fragmentation_generation.value());
    put_u64(buf, o.evidence.value());
    put_u64(buf, o.evidence_generation.value());
    put_u8(buf, static_cast<std::uint8_t>(o.provenance));
    put_u8(buf, static_cast<std::uint8_t>(o.evidence_label));
    put_u8(buf, static_cast<std::uint8_t>(o.semantics));
    put_u8(buf, static_cast<std::uint8_t>(o.state));
    put_u8(buf, static_cast<std::uint8_t>(o.busy_reason));
    put_u8(buf, static_cast<std::uint8_t>(o.idle_reason));
    put_u8(buf, static_cast<std::uint8_t>(o.classification));
    put_u8(buf, static_cast<std::uint8_t>(o.correlation));
    put_u8(buf, static_cast<std::uint8_t>(o.direction));
    put_u8(buf, static_cast<std::uint8_t>(o.device_state));
    put_u8(buf, static_cast<std::uint8_t>(o.workload_state));
    put_double(buf, o.ratio);
    put_double(buf, o.percent);
    put_u64(buf, o.bytes);
    put_u64(buf, o.capacity_bytes);
    put_u64(buf, o.count);
    put_i64(buf, o.wall_time);
}

struct Reader {
    const std::uint8_t* p;
    size_t n;
    size_t off{0};
    bool ok{true};

    std::uint8_t u8() {
        if (off + 1 > n) { ok = false; return 0; }
        return p[off++];
    }
    std::uint32_t u32() {
        if (off + 4 > n) { ok = false; return 0; }
        std::uint32_t v = 0;
        for (int i = 0; i < 4; ++i) v |= static_cast<std::uint32_t>(p[off + i]) << (8 * i);
        off += 4;
        return v;
    }
    std::uint64_t u64() {
        if (off + 8 > n) { ok = false; return 0; }
        std::uint64_t v = 0;
        for (int i = 0; i < 8; ++i) v |= static_cast<std::uint64_t>(p[off + i]) << (8 * i);
        off += 8;
        return v;
    }
    std::int64_t i64() { return static_cast<std::int64_t>(u64()); }
    double dbl() {
        std::uint64_t bits = u64();
        double d = 0;
        std::memcpy(&d, &bits, sizeof(d));
        return d;
    }
};

bool decode_observation(const std::uint8_t* data, size_t n, size_t& off_out, Observation& o) {
    Reader r{data, n};
    o.id = ObservationId(r.u64());
    o.generation = ObservationGeneration(r.u64());
    o.coordinator_epoch = CoordinatorEpoch(r.u64());
    o.type = static_cast<ObservationType>(r.u8());
    o.begin = r.u64();
    o.end = r.u64();
    o.source = SourceId(r.u64());
    o.source_generation = SourceGeneration(r.u64());
    o.device = DeviceId(r.u64());
    o.device_generation = DeviceGeneration(r.u64());
    o.host = HostId(r.u64());
    o.host_generation = HostGeneration(r.u64());
    o.worker = WorkerId(r.u64());
    o.worker_boot = WorkerBootId(r.u64());
    o.workload = WorkloadId(r.u64());
    o.workload_generation = WorkloadGeneration(r.u64());
    o.request = RequestId(r.u64());
    o.attempt = AttemptId(r.u64());
    o.attempt_generation = AttemptGeneration(r.u64());
    o.reservation = ReservationId(r.u64());
    o.reservation_generation = ReservationGeneration(r.u64());
    o.residency = ResidencyId(r.u64());
    o.residency_generation = ResidencyGeneration(r.u64());
    o.allocation = AllocationId(r.u64());
    o.allocation_generation = AllocationGeneration(r.u64());
    o.transfer = TransferId(r.u64());
    o.transfer_generation = TransferGeneration(r.u64());
    o.recovery = RecoveryId(r.u64());
    o.recovery_generation = RecoveryGeneration(r.u64());
    o.fragmentation_generation = FragmentationGeneration(r.u64());
    o.evidence = EvidenceId(r.u64());
    o.evidence_generation = EvidenceGeneration(r.u64());
    o.provenance = static_cast<Provenance>(r.u8());
    o.evidence_label = static_cast<EvidenceLabel>(r.u8());
    o.semantics = static_cast<SampleSemantics>(r.u8());
    o.state = static_cast<StateCategory>(r.u8());
    o.busy_reason = static_cast<BusyReason>(r.u8());
    o.idle_reason = static_cast<IdleReason>(r.u8());
    o.classification = static_cast<Classification>(r.u8());
    o.correlation = static_cast<CorrelationKind>(r.u8());
    o.direction = static_cast<TransferDirection>(r.u8());
    o.device_state = static_cast<DeviceState>(r.u8());
    o.workload_state = static_cast<WorkloadState>(r.u8());
    o.ratio = r.dbl();
    o.percent = r.dbl();
    o.bytes = r.u64();
    o.capacity_bytes = r.u64();
    o.count = r.u64();
    o.wall_time = r.i64();
    if (!r.ok || r.off != n) return false;
    off_out = r.off;
    return true;
}

} // namespace

namespace {

// Collect (clamped to window) intervals from a device's events matching a category.
void collect_cat(const DeviceAccum& d, StateCategory cat, const Interval& win, std::vector<Interval>& out) {
    for (const auto& e : d.events) {
        if (e.cat == cat) out.push_back(intersect(e.span, win));
    }
}

UtilizationVector compute_window_impl(const DeviceAccum& d, Interval win) {
    UtilizationVector v;
    v.window_span = win.duration();
    std::vector<Interval> useful, nonuseful, overhead, retry, membound;
    std::vector<Interval> transfer, idle, reserved, residency, fenced, unavailable, stranded;
    std::vector<Interval> waitq, waitdep, waittr, waitmem, waitrec;
    collect_cat(d, StateCategory::UsefulExecution, win, useful);
    collect_cat(d, StateCategory::NonUsefulExecution, win, nonuseful);
    collect_cat(d, StateCategory::NecessaryOverhead, win, overhead);
    collect_cat(d, StateCategory::RetryExecution, win, retry);
    collect_cat(d, StateCategory::MemoryBound, win, membound);
    collect_cat(d, StateCategory::TransferActive, win, transfer);
    collect_cat(d, StateCategory::DeviceIdle, win, idle);
    collect_cat(d, StateCategory::ReservedIdle, win, reserved);
    collect_cat(d, StateCategory::ResidencyIdle, win, residency);
    collect_cat(d, StateCategory::AuthorityFenced, win, fenced);
    collect_cat(d, StateCategory::DeviceUnavailable, win, unavailable);
    collect_cat(d, StateCategory::FragmentationStranded, win, stranded);
    collect_cat(d, StateCategory::WaitingQueue, win, waitq);
    collect_cat(d, StateCategory::WaitingDependency, win, waitdep);
    collect_cat(d, StateCategory::WaitingTransfer, win, waittr);
    collect_cat(d, StateCategory::WaitingMemory, win, waitmem);
    collect_cat(d, StateCategory::WaitingRecovery, win, waitrec);
    std::vector<Interval> busy;
    busy.insert(busy.end(), useful.begin(), useful.end());
    busy.insert(busy.end(), nonuseful.begin(), nonuseful.end());
    busy.insert(busy.end(), overhead.begin(), overhead.end());
    busy.insert(busy.end(), retry.begin(), retry.end());
    busy.insert(busy.end(), membound.begin(), membound.end());
    v.compute_busy_time = union_duration(busy);
    v.useful_compute_time = union_duration(useful);
    v.non_useful_compute_time = union_duration(nonuseful);
    v.necessary_overhead_time = union_duration(overhead);
    v.retry_time = union_duration(retry);
    v.memory_pressure_time = union_duration(membound);
    v.transfer_active_time = union_duration(transfer);
    v.compute_idle_time = union_duration(idle);
    v.reserved_idle_time = union_duration(reserved);
    v.residency_idle_time = union_duration(residency);
    v.fenced_time = union_duration(fenced);
    v.unavailable_time = union_duration(unavailable);
    v.stranded_capacity_time = union_duration(stranded);
    v.queue_wait_time = union_duration(waitq);
    v.dependency_wait_time = union_duration(waitdep);
    v.waiting_transfer_time = union_duration(waittr);
    v.waiting_memory_time = union_duration(waitmem);
    v.recovery_wait_time = union_duration(waitrec);
    v.compute_transfer_overlap_time = intersect_union_duration(busy, transfer);
    return v;
}

} // namespace

UtilizationVector UtilizationObservatory::device_window(DeviceId dev, Interval win) const {
    std::shared_lock<std::shared_mutex> lock(impl_->mtx);
    auto dit = impl_->devices.find(dev);
    if (dit == impl_->devices.end()) {
        UtilizationVector v;
        v.window_span = win.duration();
        return v;
    }
    return compute_window_impl(dit->second, win);
}

UtilizationGap UtilizationObservatory::gap(DeviceId dev, Interval win) const {
    std::shared_lock<std::shared_mutex> lock(impl_->mtx);
    UtilizationGap g;
    g.device = dev;
    g.window_begin = win.begin;
    g.window_end = win.end;
    Tick span = win.duration();
    auto dit = impl_->devices.find(dev);
    if (dit == impl_->devices.end()) {
        if (span != 0) g.unknown = Ratio{static_cast<double>(span), static_cast<double>(span)};
        return g;  // no evidence -> the whole window is UNKNOWN
    }
    const auto& d = dit->second;
    if (span == 0) return g;

    UtilizationVector v = compute_window_impl(d, win);
    double s = static_cast<double>(span);
    g.headline_busy = Ratio{static_cast<double>(v.compute_busy_time), s};
    g.useful_execution = Ratio{static_cast<double>(v.useful_compute_time), s};
    g.non_useful_busy = Ratio{static_cast<double>(v.non_useful_compute_time + v.necessary_overhead_time + v.retry_time), s};
    g.overhead = Ratio{static_cast<double>(v.necessary_overhead_time), s};
    g.idle = Ratio{static_cast<double>(v.compute_idle_time), s};
    g.reserved_idle = Ratio{static_cast<double>(v.reserved_idle_time), s};
    g.stranded_capacity = Ratio{static_cast<double>(v.stranded_capacity_time), s};
    g.gap_points = (g.headline_busy.value() - g.useful_execution.value()) * 100.0;

    // UNKNOWN is the window not attributable to any categorized interval.
    std::vector<Interval> accounted;
    for (const auto& e : d.events) accounted.push_back(intersect(e.span, win));
    Tick accounted_dur = union_duration(accounted);
    double unknown = s - static_cast<double>(accounted_dur);
    if (unknown < 0.0) unknown = 0.0;
    g.unknown = Ratio{unknown, s};

    auto add_contrib = [&](StateCategory cat, double dur) {
        if (dur > 0.0) {
            Contribution c;
            c.category = cat;
            c.ratio = dur / s;
            c.raw_time = dur;
            c.provenance = Provenance::Derived;
            g.contributors.push_back(c);
        }
    };
    add_contrib(StateCategory::UsefulExecution, static_cast<double>(v.useful_compute_time));
    add_contrib(StateCategory::NonUsefulExecution, static_cast<double>(v.non_useful_compute_time));
    add_contrib(StateCategory::NecessaryOverhead, static_cast<double>(v.necessary_overhead_time));
    add_contrib(StateCategory::RetryExecution, static_cast<double>(v.retry_time));
    add_contrib(StateCategory::TransferActive, static_cast<double>(v.transfer_active_time));
    add_contrib(StateCategory::ReservedIdle, static_cast<double>(v.reserved_idle_time));
    add_contrib(StateCategory::ResidencyIdle, static_cast<double>(v.residency_idle_time));
    add_contrib(StateCategory::FragmentationStranded, static_cast<double>(v.stranded_capacity_time));
    add_contrib(StateCategory::DeviceIdle, static_cast<double>(v.compute_idle_time));
    add_contrib(StateCategory::WaitingQueue, static_cast<double>(v.queue_wait_time));
    add_contrib(StateCategory::WaitingDependency, static_cast<double>(v.dependency_wait_time));
    add_contrib(StateCategory::WaitingRecovery, static_cast<double>(v.recovery_wait_time));
    std::sort(g.contributors.begin(), g.contributors.end(),
              [](const Contribution& a, const Contribution& b) {
                  if (a.ratio != b.ratio) return a.ratio > b.ratio;
                  return static_cast<int>(a.category) < static_cast<int>(b.category);
              });
    return g;
}

UtilizationExplanation UtilizationObservatory::explain(DeviceId dev, Interval win) const {
    std::shared_lock<std::shared_mutex> lock(impl_->mtx);
    UtilizationExplanation ex;
    ex.device = dev;
    ex.span = win;
    auto dit = impl_->devices.find(dev);
    if (dit == impl_->devices.end()) {
        ex.unknown_reasons.push_back("no evidence for device");
        return ex;
    }
    const auto& d = dit->second;
    Tick span = win.duration();
    if (span == 0) return ex;
    double s = static_cast<double>(span);
    UtilizationVector v = compute_window_impl(d, win);
    ex.headline_busy = static_cast<double>(v.compute_busy_time) / s;
    ex.useful_busy = static_cast<double>(v.useful_compute_time) / s;
    ex.non_useful_busy = static_cast<double>(v.non_useful_compute_time + v.necessary_overhead_time + v.retry_time) / s;
    ex.necessary_overhead = static_cast<double>(v.necessary_overhead_time) / s;
    ex.unknown_busy = 0.0;
    ex.idle = static_cast<double>(v.compute_idle_time) / s;
    ex.reserved_idle = static_cast<double>(v.reserved_idle_time) / s;
    ex.stranded_capacity = static_cast<double>(v.stranded_capacity_time) / s;

    bool has_useful = v.useful_compute_time > 0 || !d.useful_bands.empty();
    if (!has_useful) {
        ex.unknown_reasons.push_back(
            "no Efficiency Ledger-compatible useful-work evidence; useful busy is UNKNOWN rather than zero");
    }
    if (!d.vendor_defined) {
        ex.unknown_reasons.push_back(
            "no vendor percentile sample present; headline busy is reconstructed from execution intervals");
    }
    if (ex.headline_busy > 0.0 && ex.useful_busy == 0.0 && has_useful) {
        ex.unknown_reasons.push_back("useful work classified as non-useful requires interning attribution");
    }
    return ex;
}

DeviceSnapshot UtilizationObservatory::snapshot(DeviceId dev, Tick t) const {
    std::shared_lock<std::shared_mutex> lock(impl_->mtx);
    DeviceSnapshot s;
    s.device = dev;
    s.at = t;
    auto dit = impl_->devices.find(dev);
    if (dit == impl_->devices.end()) {
        s.state = DeviceState::Unknown;
        s.freshness = Freshness::Unknown;
        return s;
    }
    const auto& d = dit->second;
    s.generation = impl_->device_gens.count(dev) ? impl_->device_gens[dev] : DeviceGeneration(0);
    s.id = SnapshotId(1);
    s.state = d.physical;
    s.capacity = d.cap;
    s.source = d.authoritative;
    s.source_generation = d.authoritative_gen;
    s.vendor_percent = d.last_vendor_percent;
    s.vendor_percent_defined = d.vendor_defined;
    s.wall_time = d.last_wall;
    s.dims = compute_window_impl(d, Interval{t, t});
    s.dims.window_span = 0;

    auto srcIt = impl_->sources.find(d.authoritative);
    Freshness fresh = Freshness::Unknown;
    if (srcIt != impl_->sources.end()) {
        fresh = impl_->eval_freshness(srcIt->second, impl_->now());
        s.provenance = impl_->devices.count(dev) ? s.provenance : s.provenance;
    }
    s.freshness = fresh;
    s.consistent = (fresh == Freshness::Current);
    return s;
}

WorkloadSnapshot UtilizationObservatory::workload(WorkloadId wl, Interval win) const {
    std::shared_lock<std::shared_mutex> lock(impl_->mtx);
    WorkloadSnapshot ws;
    ws.workload = wl;
    std::vector<Interval> active, qwait, retry, transfer, residency, reservation, useful, nonuseful, blocked;
    for (const auto& kv : impl_->devices) {
        for (const auto& e : kv.second.events) {
            if (e.wl != wl) continue;
            Interval x = intersect(e.span, win);
            switch (e.cat) {
                case StateCategory::UsefulExecution: useful.push_back(x); break;
                case StateCategory::NonUsefulExecution: nonuseful.push_back(x); break;
                case StateCategory::RetryExecution: retry.push_back(x); break;
                case StateCategory::TransferActive: transfer.push_back(x); break;
                case StateCategory::ResidencyIdle: residency.push_back(x); break;
                case StateCategory::ReservedIdle: reservation.push_back(x); break;
                default: break;
            }
        }
    }
    active = useful; active.insert(active.end(), nonuseful.begin(), nonuseful.end());
    active.insert(active.end(), retry.begin(), retry.end());
    ws.active_execution = union_duration(active);
    ws.useful_execution = union_duration(useful);
    ws.non_useful_execution = union_duration(nonuseful);
    ws.retry_time = union_duration(retry);
    ws.transfer_time = union_duration(transfer);
    ws.residency_held = union_duration(residency);
    ws.reservation_held = union_duration(reservation);
    return ws;
}

std::vector<TimelineEvent> UtilizationObservatory::timeline(Interval win) const {
    std::shared_lock<std::shared_mutex> lock(impl_->mtx);
    std::vector<TimelineEvent> out;
    for (const auto& kv : impl_->devices) {
        for (const auto& e : kv.second.events) {
            Interval x = intersect(e.span, win);
            if (x.duration() == 0 && !(win.contains(e.span.begin))) continue;
            TimelineEvent te;
            te.span = e.span;
            te.source = e.src;
            te.source_generation = e.srcgen;
            te.device = kv.first;
            te.device_generation = e.devgen;
            te.workload = e.wl;
            te.workload_generation = e.wlgen;
            te.state = e.cat;
            te.provenance = e.prov;
            te.freshness = e.fresh;
            te.observation = e.obs;
            out.push_back(te);
        }
    }
    std::sort(out.begin(), out.end(), [](const TimelineEvent& a, const TimelineEvent& b) {
        if (a.span.begin != b.span.begin) return a.span.begin < b.span.begin;
        if (a.device.value() != b.device.value()) return a.device.value() < b.device.value();
        if (a.source.value() != b.source.value()) return a.source.value() < b.source.value();
        return a.observation.value() < b.observation.value();
    });
    return out;
}

CapacityState UtilizationObservatory::capacity(DeviceId dev, Tick t) const {
    (void)t;
    std::shared_lock<std::shared_mutex> lock(impl_->mtx);
    auto dit = impl_->devices.find(dev);
    if (dit == impl_->devices.end()) return CapacityState{};
    return dit->second.cap;
}

BackendCapabilities UtilizationObservatory::capabilities(DeviceId dev) const {
    std::shared_lock<std::shared_mutex> lock(impl_->mtx);
    auto dit = impl_->devices.find(dev);
    if (dit == impl_->devices.end()) return BackendCapabilities{};
    return dit->second.caps;
}

SourceInfo UtilizationObservatory::source_health(SourceId id) const {
    std::shared_lock<std::shared_mutex> lock(impl_->mtx);
    auto it = impl_->sources.find(id);
    if (it == impl_->sources.end()) return SourceInfo{};
    Tick now = impl_->now();
    SourceInfo info = it->second.info;
    info.health = impl_->eval_health(it->second, now);
    info.freshness = impl_->eval_freshness(it->second, now);
    return info;
}

std::vector<SourceInfo> UtilizationObservatory::all_sources() const {
    std::shared_lock<std::shared_mutex> lock(impl_->mtx);
    std::vector<SourceInfo> out;
    Tick now = impl_->now();
    for (const auto& kv : impl_->sources) {
        SourceInfo info = kv.second.info;
        info.health = impl_->eval_health(kv.second, now);
        info.freshness = impl_->eval_freshness(kv.second, now);
        out.push_back(std::move(info));
    }
    std::sort(out.begin(), out.end(), [](const SourceInfo& a, const SourceInfo& b) {
        return a.id.value() < b.id.value();
    });
    return out;
}

std::vector<DeviceId> UtilizationObservatory::all_devices() const {
    std::shared_lock<std::shared_mutex> lock(impl_->mtx);
    std::vector<DeviceId> out;
    for (const auto& kv : impl_->devices) out.push_back(kv.first);
    std::sort(out.begin(), out.end(), [](DeviceId a, DeviceId b) { return a.value() < b.value(); });
    return out;
}

UtilizationObservatory::ObservatoryHealth UtilizationObservatory::health() const {
    std::lock_guard<std::shared_mutex> lock(impl_->mtx);
    ObservatoryHealth h;
    h.accepted = impl_->a_accepted.load(std::memory_order_relaxed);
    h.rejected = impl_->a_rejected.load(std::memory_order_relaxed);
    h.duplicates = impl_->a_duplicates.load(std::memory_order_relaxed);
    h.stale = impl_->a_stale.load(std::memory_order_relaxed);
    h.source_disconnects = impl_->a_disconnects.load(std::memory_order_relaxed);
    h.source_reconnects = impl_->a_reconnects.load(std::memory_order_relaxed);
    h.persistence_errors = impl_->a_persist_errors.load(std::memory_order_relaxed);
    h.backend_errors = impl_->a_backend_errors.load(std::memory_order_relaxed);
    h.dropped_bounds = impl_->a_dropped.load(std::memory_order_relaxed);
    h.source_count = impl_->sources.size();
    h.device_count = impl_->devices.size();
    return h;
}

Status UtilizationObservatory::save(const std::string& path) const {
    std::vector<Observation> obs;
    {
        std::shared_lock<std::shared_mutex> lock(impl_->mtx);
        obs.assign(impl_->history.begin(), impl_->history.end());
    }
    std::vector<std::uint8_t> body;
    put_u32(body, 1u);                        // format version
    put_u64(body, impl_->epoch.value());
    put_u64(body, impl_->generation.value());
    put_u64(body, obs.size());
    for (const auto& o : obs) {
        std::vector<std::uint8_t> enc;
        encode_observation(enc, o);
        put_u32(body, static_cast<std::uint32_t>(enc.size()));
        body.insert(body.end(), enc.begin(), enc.end());
        put_u32(body, crc32c(enc.data(), enc.size()));
    }
    const std::string tmp = path + ".tmp";
    {
        std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
        if (!f) {
            impl_->a_persist_errors.fetch_add(1, std::memory_order_relaxed);
            return Status(StatusCode::PersistenceCorrupt, "cannot open temp file");
        }
        const char magic[4] = {'U', 'O', 'B', 'F'};
        f.write(magic, 4);
        f.write(reinterpret_cast<const char*>(body.data()),
                static_cast<std::streamsize>(body.size()));
        f.flush();
        if (!f.good()) {
            f.close();
            std::remove(tmp.c_str());
            impl_->a_persist_errors.fetch_add(1, std::memory_order_relaxed);
            return Status(StatusCode::PersistenceCorrupt, "write failed");
        }
        f.close();
    }
    std::error_code ec;
    std::filesystem::rename(tmp, path, ec);
    if (ec) {
        std::filesystem::remove(path, ec);
        std::filesystem::rename(tmp, path, ec);
        if (ec) {
            std::filesystem::remove(tmp.c_str(), ec);
            impl_->a_persist_errors.fetch_add(1, std::memory_order_relaxed);
            return Status(StatusCode::PersistenceCorrupt, "rename failed");
        }
    }
    return Status::success();
}

Status UtilizationObservatory::load(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return Status(StatusCode::PersistenceCorrupt, "cannot open");
    char magic[4];
    f.read(magic, 4);
    if (!f || magic[0] != 'U' || magic[1] != 'O' || magic[2] != 'B' || magic[3] != 'F')
        return Status(StatusCode::PersistenceCorrupt, "bad magic");
    std::vector<std::uint8_t> body{std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
    Reader r{body.data(), body.size()};
    std::uint32_t version = r.u32();
    if (!r.ok) return Status(StatusCode::PersistenceCorrupt, "truncated header");
    if (version != 1u) return Status(StatusCode::PersistenceCorrupt, "unknown version");
    (void)r.u64();  // persisted epoch (informational; the running epoch is kept)
    (void)r.u64();  // persisted generation
    std::uint64_t count = r.u64();
    if (!r.ok || count > (body.size() / 2)) return Status(StatusCode::PersistenceCorrupt, "bad count");
    std::vector<Observation> obs;
    obs.reserve(static_cast<std::size_t>(count));
    for (std::uint64_t i = 0; i < count; ++i) {
        std::uint32_t len = r.u32();
        if (!r.ok || len == 0 || len > 16u * 1024u * 1024u) return Status(StatusCode::PersistenceCorrupt, "bad len");
        if (r.off + len + 4 > r.n) return Status(StatusCode::PersistenceCorrupt, "truncated record");
        const std::uint8_t* p = r.p + r.off;
        std::uint32_t actual = crc32c(p, len);
        r.off += len;
        std::uint32_t stored = r.u32();
        if (!r.ok || actual != stored) return Status(StatusCode::PersistenceCorrupt, "crc mismatch");
        Observation o;
        std::size_t oo = 0;
        if (!decode_observation(p, len, oo, o)) return Status(StatusCode::PersistenceCorrupt, "decode");
        obs.push_back(o);
    }
    if (r.off != r.n) return Status(StatusCode::PersistenceCorrupt, "trailing garbage");

    std::unique_lock<std::shared_mutex> lock(impl_->mtx);
    auto& I = *impl_;
    I.sources.clear();
    I.devices.clear();
    I.workload_gens.clear();
    I.evidence_gens.clear();
    I.device_gens.clear();
    I.history.clear();
    I.dedup.clear();
    I.dedup_order.clear();
    I.a_accepted.store(0, std::memory_order_relaxed);
    I.a_rejected.store(0, std::memory_order_relaxed);
    I.a_duplicates.store(0, std::memory_order_relaxed);
    I.a_stale.store(0, std::memory_order_relaxed);
    I.a_disconnects.store(0, std::memory_order_relaxed);
    I.a_reconnects.store(0, std::memory_order_relaxed);
    I.a_persist_errors.store(0, std::memory_order_relaxed);
    I.a_backend_errors.store(0, std::memory_order_relaxed);
    I.a_dropped.store(0, std::memory_order_relaxed);
    I.historical_mode = true;
    Status st = Status::success();
    for (const auto& o : obs) {
        Status s = ingest_locked(o);
        if (!s.ok()) { st = s; break; }
    }
    I.historical_mode = false;
    for (auto& kv : I.sources) {
        kv.second.connected = false;
        kv.second.info.health = SourceHealth::Disconnected;
        kv.second.info.freshness = Freshness::RevalidationRequired;
    }
    return st;
}

std::uint64_t UtilizationObservatory::canonical_digest() const {
    std::shared_lock<std::shared_mutex> lock(impl_->mtx);
    std::vector<Observation> obs(impl_->history.begin(), impl_->history.end());
    std::sort(obs.begin(), obs.end(), [](const Observation& a, const Observation& b) {
        if (a.begin != b.begin) return a.begin < b.begin;
        if (a.source.value() != b.source.value()) return a.source.value() < b.source.value();
        return a.id.value() < b.id.value();
    });
    Digest64 d;
    for (const auto& o : obs) {
        std::vector<std::uint8_t> enc;
        encode_observation(enc, o);
        d.put(enc.data(), enc.size());
    }
    return d.value();
}

ReplayResult UtilizationObservatory::replay(const std::string& path) const {
    ReplayResult res;
    res.persistence_path = path;
    UtilizationObservatory clone(cfg_);
    Status s = clone.load(path);
    if (!s.ok()) {
        res.observations_replayed = 0;
        res.digests_equal = false;
        return res;
    }
    res.observations_replayed = clone.health().accepted;
    res.digest_a = canonical_digest();
    res.digest_b = clone.canonical_digest();
    res.digests_equal = (res.digest_a == res.digest_b);
    (void)s;
    return res;
}

} // namespace uo



