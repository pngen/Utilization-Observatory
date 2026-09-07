#pragma once
// Utilization Observatory : clock / time model.
// We use an injectable monotonic clock for deterministic tests. Monotonic runtime
// time is the ordering authority; wall-clock presentation time is separate and is
// never used for correctness ordering.
// Copyright 2026 Summon Software Labs. Apache-2.0.

#include <cstdint>

namespace uo {

// Monotonic runtime tick, in nanoseconds relative to an unspecified origin. It is
// the authoritative ordering domain for evidence. A zero tick is a valid neutral
// origin; callers that require a positive origin should store an explicit epoch.
using Tick = std::uint64_t;

// Wall-clock presentation time (Unix epoch seconds). Used only for display and for
// snapshot provenance, not for correctness ordering.
using WallTime = std::int64_t;

// Injectable monotonic clock. Not thread-safe by default; callers that sample from
// multiple threads should wrap it or use the plain clock.
class MonotonicClock {
public:
    virtual ~MonotonicClock() = default;
    virtual Tick now_tick() const = 0;
};

// The default clock uses the OS steady clock. It is injectable so tests can supply
// a scripted sequence of ticks.
class SteadyClock final : public MonotonicClock {
public:
    Tick now_tick() const override;
};

// A hand-driven clock for deterministic tests. Not thread-safe.
class ScriptedClock final : public MonotonicClock {
public:
    explicit ScriptedClock(Tick start = 0) : t_(start) {}
    Tick now_tick() const override { return t_; }
    void set(Tick t) noexcept { t_ = t; }
    void advance(Tick delta) noexcept { t_ += delta; }
private:
    Tick t_{0};
};

// Wall-clock presentation clock (seconds since Unix epoch). Injectable for tests.
class WallClock {
public:
    virtual ~WallClock() = default;
    virtual WallTime now_wall() const = 0;
};

class SystemWallClock final : public WallClock {
public:
    WallTime now_wall() const override;
};

} // namespace uo
