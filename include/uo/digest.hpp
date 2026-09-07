#pragma once
// Utilization Observatory : deterministic canonical digest.
// FNV-1a 64-bit over the canonical, semantically meaningful observatory state.
// Memory addresses, unordered iteration order, temporary names, wall-clock noise,
// and ephemeral connection handles are deliberately excluded by construction.
// Copyright 2026 Summon Software Labs. Apache-2.0.

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

namespace uo {

class Digest64 {
public:
    Digest64() = default;
    explicit Digest64(std::uint64_t seed) : h_(seed) {}

    void put(const void* data, std::size_t n) noexcept {
        const auto* p = static_cast<const std::uint8_t*>(data);
        for (std::size_t i = 0; i < n; ++i) {
            h_ ^= p[i];
            h_ *= 0x100000001b3ull;
        }
    }

    void put_u8(std::uint8_t v) noexcept { put(&v, 1); }
    void put_u32(std::uint32_t v) noexcept { put(&v, sizeof(v)); }
    void put_u64(std::uint64_t v) noexcept { put(&v, sizeof(v)); }
    void put_i64(std::int64_t v) noexcept { put(&v, sizeof(v)); }
    void put_bool(bool v) noexcept { std::uint8_t b = v ? 1 : 0; put(&b, 1); }

    // Deterministic double hashing: hash the IEEE-754 bit pattern, not any
    // textual representation, so the digest is identical across platforms.
    void put_double(double v) noexcept {
        std::uint64_t bits = 0;
        std::memcpy(&bits, &v, sizeof(v));
        put_u64(bits);
    }

    void put_str(const std::string& s) noexcept {
        put(s.data(), s.size());
    }

    std::uint64_t value() const noexcept { return h_; }

private:
    std::uint64_t h_{0xcbf29ce484222325ull};
};

} // namespace uo
