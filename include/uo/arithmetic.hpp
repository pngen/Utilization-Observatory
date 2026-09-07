#pragma once
// Utilization Observatory : checked arithmetic for authoritative counters.
// All interval/capacity counters must never wrap. These helpers are used for
// duration accumulation, byte-time, device-time, capacity-time, ratio numerators,
// window totals, and cross-source aggregation.
// Copyright 2026 Summon Software Labs. Apache-2.0.

#include <cstdint>

namespace uo {

constexpr bool checked_add_u64(std::uint64_t a, std::uint64_t b, std::uint64_t& out) noexcept {
    if (b > UINT64_MAX - a) return false;
    out = a + b;
    return true;
}

constexpr bool checked_mul_u64(std::uint64_t a, std::uint64_t b, std::uint64_t& out) noexcept {
    if (a != 0 && b > UINT64_MAX / a) return false;
    out = a * b;
    return true;
}

// Saturating variants: never overflow, never false. Prefer the checked variant in
// authoritative accounting; saturating is for display/best-effort paths.
constexpr std::uint64_t sat_add_u64(std::uint64_t a, std::uint64_t b) noexcept {
    return (b > UINT64_MAX - a) ? UINT64_MAX : a + b;
}

constexpr std::uint64_t sat_mul_u64(std::uint64_t a, std::uint64_t b) noexcept {
    if (a == 0 || b == 0) return 0;
    if (b > UINT64_MAX / a) return UINT64_MAX;
    return a * b;
}

} // namespace uo
