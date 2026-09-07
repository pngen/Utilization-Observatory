#pragma once
// Utilization Observatory : framed TCP transport protocol.
// Frame = [version u32][total_length u32][crc32c u32][payload]. Version and length
// bounds are enforced; the CRC rejects corruption/truncation; trailing garbage and
// unknown version are rejected. Partial reads/writes are handled by the socket
// layer. Copyright 2026 Summon Software Labs. Apache-2.0.
#include <cstdint>
#include <vector>

namespace uo {
namespace dist {

constexpr std::uint32_t kFrameVersion = 1;
constexpr std::uint32_t kMaxFrame = 16u * 1024u * 1024u;
constexpr std::uint32_t kHeaderSize = 12;

enum class MsgKind : std::uint8_t {
    Hello = 1,
    Observation = 2,
    Query = 3,
    Snapshot = 4,
    Close = 5,
    HelloReply = 6,
};

struct HelloPayload {
    std::uint32_t worker_id{0};
    std::uint64_t boot_id{0};
    std::uint32_t source_generation{0};
};

struct SnapshotPayload {
    std::uint64_t accepted{0};
    std::uint64_t busy_ns{0};
    std::uint64_t useful_ns{0};
    std::uint64_t reserved_ns{0};
    std::uint64_t stranded_ns{0};
    std::uint64_t digest{0};
    std::uint32_t sources{0};
    char text[512]{0};
};

struct Frame {
    MsgKind kind{MsgKind::Close};
    std::vector<std::uint8_t> body;
};

enum class DecodeResult { Ok, Incomplete, Corrupt };

DecodeResult try_decode(const std::uint8_t* data, std::size_t n, std::size_t& consumed, Frame& out);
bool encode_frame(MsgKind kind, const std::vector<std::uint8_t>& payload, std::vector<std::uint8_t>& out);

void put_u32(std::vector<std::uint8_t>& b, std::uint32_t v);
void put_u64(std::vector<std::uint8_t>& b, std::uint64_t v);
std::uint32_t get_u32(const std::vector<std::uint8_t>& b, std::size_t& off);
std::uint64_t get_u64(const std::vector<std::uint8_t>& b, std::size_t& off);

class FramedConnection {
public:
    explicit FramedConnection(std::uintptr_t sock);
    ~FramedConnection();
    FramedConnection(const FramedConnection&) = delete;
    FramedConnection& operator=(const FramedConnection&) = delete;

    bool send_frame(MsgKind kind, const std::vector<std::uint8_t>& payload);
    bool recv_frame(Frame& out, bool& closed);

private:
    std::uintptr_t sock_;
    std::vector<std::uint8_t> rbuf_;
};

} // namespace dist
} // namespace uo
