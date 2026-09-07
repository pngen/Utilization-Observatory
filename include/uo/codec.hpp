#pragma once
// Utilization Observatory : wire/persistence observation codec.
// Deterministic little-endian encoding of an Observation for the framed protocol
// and persistence. Bounded decode is the caller's responsibility.
// Copyright 2026 Summon Software Labs. Apache-2.0.
#include "uo/observation.hpp"
#include <cstdint>
#include <cstddef>
#include <vector>

namespace uo {

void encode_observation(std::vector<std::uint8_t>& buf, const Observation& o);
bool decode_observation(const std::uint8_t* data, std::size_t n, std::size_t& off_out, Observation& o);

} // namespace uo
