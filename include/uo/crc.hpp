#pragma once
// Utilization Observatory : CRC-32C (Castagnoli) for persistence integrity.
// Reflected polynomial 0x82F63B78. Strong enough for corruption/truncation
// detection; trailing garbage and unknown-version rejection are enforced by the
// format layer. Computed bit-by-bit so it is exact and has no table edge cases.
// Copyright 2026 Summon Software Labs. Apache-2.0.

#include <cstddef>
#include <cstdint>

namespace uo {

inline std::uint32_t crc32c(std::uint32_t seed, const void* data, std::size_t len) noexcept {
    std::uint32_t crc = seed;
    const auto* p = static_cast<const std::uint8_t*>(data);
    for (std::size_t i = 0; i < len; ++i) {
        crc ^= p[i];
        for (int b = 0; b < 8; ++b) {
            crc = (crc >> 1) ^ ((crc & 1u) ? 0x82F63B78u : 0x00000000u);
        }
    }
    return crc;
}

inline std::uint32_t crc32c(const void* data, std::size_t len) noexcept {
    return crc32c(0xFFFFFFFFu, data, len) ^ 0xFFFFFFFFu;
}

} // namespace uo
