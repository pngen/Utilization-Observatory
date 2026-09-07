#pragma once
// Utilization Observatory : exact interval model.
// Intervals are half-open [begin, end) over monotonic ticks. Exact intersection
// and overlap accounting are provided; no overlap is double-counted. Zero-length
// intervals are allowed but contribute zero duration.
// Copyright 2026 Summon Software Labs. Apache-2.0.

#include "uo/time.hpp"
#include <algorithm>

namespace uo {

struct Interval {
    Tick begin{0};
    Tick end{0};

    constexpr bool valid() const noexcept { return end >= begin; }
    constexpr bool is_empty() const noexcept { return end <= begin; }
    constexpr Tick duration() const noexcept { return end >= begin ? end - begin : 0; }
    constexpr bool contains(Tick t) const noexcept { return t >= begin && t < end; }
    constexpr bool operator==(const Interval& o) const noexcept { return begin == o.begin && end == o.end; }
    constexpr bool operator!=(const Interval& o) const noexcept { return !(*this == o); }

    static constexpr Interval make(Tick b, Tick e) noexcept { return Interval{b, e}; }
};

// Exact intersection; if the intervals do not overlap the result is empty.
constexpr Interval intersect(Interval a, Interval b) noexcept {
    Tick lo = std::max(a.begin, b.begin);
    Tick hi = std::min(a.end, b.end);
    if (hi < lo) hi = lo;
    return Interval{lo, hi};
}

// Overlap duration between two intervals (exactly the length of the intersection).
constexpr Tick overlap_duration(Interval a, Interval b) noexcept {
    return intersect(a, b).duration();
}

// Whether one interval is strictly before another in time.
constexpr bool before(Interval a, Interval b) noexcept { return a.end <= b.begin; }

} // namespace uo
