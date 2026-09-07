#include "protocol.hpp"
#include "uo/crc.hpp"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <cstring>

namespace uo {
namespace dist {

void put_u32(std::vector<std::uint8_t>& b, std::uint32_t v) {
    for (int i = 0; i < 4; ++i) b.push_back(static_cast<std::uint8_t>((v >> (8 * i)) & 0xFFu));
}
void put_u64(std::vector<std::uint8_t>& b, std::uint64_t v) {
    for (int i = 0; i < 8; ++i) b.push_back(static_cast<std::uint8_t>((v >> (8 * i)) & 0xFFu));
}
std::uint32_t get_u32(const std::vector<std::uint8_t>& b, std::size_t& off) {
    std::uint32_t v = 0;
    for (int i = 0; i < 4; ++i) v |= static_cast<std::uint32_t>(b[off + i]) << (8 * i);
    off += 4;
    return v;
}
std::uint64_t get_u64(const std::vector<std::uint8_t>& b, std::size_t& off) {
    std::uint64_t v = 0;
    for (int i = 0; i < 8; ++i) v |= static_cast<std::uint64_t>(b[off + i]) << (8 * i);
    off += 8;
    return v;
}

static std::uint32_t get_u32_c(const std::uint8_t* p, std::size_t& off);

bool encode_frame(MsgKind kind, const std::vector<std::uint8_t>& payload, std::vector<std::uint8_t>& out) {
    std::vector<std::uint8_t> full;
    full.push_back(static_cast<std::uint8_t>(kind));
    full.insert(full.end(), payload.begin(), payload.end());
    if (full.size() > kMaxFrame - kHeaderSize) return false;
    out.clear();
    put_u32(out, kFrameVersion);
    put_u32(out, static_cast<std::uint32_t>(full.size()));
    put_u32(out, crc32c(full.data(), full.size()));
    out.insert(out.end(), full.begin(), full.end());
    return true;
}

DecodeResult try_decode(const std::uint8_t* data, std::size_t n, std::size_t& consumed, Frame& out) {
    consumed = 0;
    if (n < kHeaderSize) return DecodeResult::Incomplete;
    std::size_t off = 0;
    std::uint32_t version = get_u32_c(data, off);
    std::uint32_t length = get_u32_c(data, off);
    std::uint32_t crc = get_u32_c(data, off);
    if (version != kFrameVersion) return DecodeResult::Corrupt;   // unknown version
    if (length > kMaxFrame) return DecodeResult::Corrupt;         // oversize
    if (n < kHeaderSize + length) return DecodeResult::Incomplete;
    const std::uint8_t* payload = data + kHeaderSize;
    if (crc32c(payload, length) != crc) return DecodeResult::Corrupt;   // checksum
    if (length < 1) return DecodeResult::Corrupt;                // malformed (no kind byte)
    out.kind = static_cast<MsgKind>(payload[0]);
    out.body.assign(payload + 1, payload + length);
    consumed = kHeaderSize + length;
    return DecodeResult::Ok;
}

static std::uint32_t get_u32_c(const std::uint8_t* p, std::size_t& off) {
    std::uint32_t v = 0;
    for (int i = 0; i < 4; ++i) v |= static_cast<std::uint32_t>(p[off + i]) << (8 * i);
    off += 4;
    return v;
}

FramedConnection::FramedConnection(std::uintptr_t sock) : sock_(sock) {}
FramedConnection::~FramedConnection() {
    if (sock_) ::closesocket(static_cast<SOCKET>(sock_));
}

bool FramedConnection::send_frame(MsgKind kind, const std::vector<std::uint8_t>& payload) {
    std::vector<std::uint8_t> frame;
    if (!encode_frame(kind, payload, frame)) return false;
    SOCKET s = static_cast<SOCKET>(sock_);
    std::size_t sent = 0;
    while (sent < frame.size()) {
        int n = ::send(s, reinterpret_cast<const char*>(frame.data()) + sent,
                       static_cast<int>(frame.size() - sent), 0);
        if (n <= 0) return false;   // handle WSAEINTR by treating as error (loopback not interrupted)
        sent += static_cast<std::size_t>(n);
    }
    return true;
}

bool FramedConnection::recv_frame(Frame& out, bool& closed) {
    closed = false;
    SOCKET s = static_cast<SOCKET>(sock_);
    for (;;) {
        std::size_t consumed = 0;
        Frame f;
        DecodeResult d = try_decode(rbuf_.data(), rbuf_.size(), consumed, f);
        if (d == DecodeResult::Ok) {
            out = std::move(f);
            rbuf_.erase(rbuf_.begin(), rbuf_.begin() + static_cast<std::ptrdiff_t>(consumed));
            return true;
        }
        if (d == DecodeResult::Corrupt) return false;   // protocol error
        // Incomplete: read more.
        char buf[4096];
        int n = ::recv(s, buf, sizeof(buf), 0);
        if (n == 0) {   // orderly shutdown
            if (rbuf_.empty()) { closed = true; return false; }
            return false;   // truncated frame => corrupt
        }
        if (n < 0) {
            // Check for interrupted/continue; loopback rarely sees this.
            if (sock_ && WSAGetLastError() == WSAEWOULDBLOCK) { closed = true; return false; }
            return false;
        }
        rbuf_.insert(rbuf_.end(), buf, buf + n);
        if (rbuf_.size() > kMaxFrame + kHeaderSize) return false;   // oversize guard
    }
}

} // namespace dist
} // namespace uo
