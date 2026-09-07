// Utilization Observatory : distributed coordinator (real OS process, TCP).
// Thread-per-connection so multiple workers and query clients are served
// concurrently. Copyright 2026 Summon Software Labs. Apache-2.0.
#include "protocol.hpp"
#include "uo/codec.hpp"
#include "uo/observatory.hpp"
#include "uo/source.hpp"
#include "uo/time.hpp"
#include "uo/view.hpp"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

using namespace uo;
using namespace uo::dist;

static SteadyClock clk;
static std::mutex g_save;

static SOCKET bind_listen(int port) {
    SOCKET s = ::socket(AF_INET, SOCK_STREAM, 0);
    if (s == INVALID_SOCKET) return INVALID_SOCKET;
    int on = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&on), sizeof(on));
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(static_cast<u_short>(port));
    if (::bind(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) { ::closesocket(s); return INVALID_SOCKET; }
    if (::listen(s, 64) == SOCKET_ERROR) { ::closesocket(s); return INVALID_SOCKET; }
    return s;
}

int main(int argc, char** argv) {
    if (argc < 3) { std::printf("usage: uo_coordinator <port> <persist_path>\n"); return 1; }
    int port = std::atoi(argv[1]);
    const std::string persist = argv[2];

    WSADATA w; WSAStartup(MAKEWORD(2, 2), &w);

    UtilizationObservatory::Config cfg;
    UtilizationObservatory obs(cfg);
    if (std::ifstream(persist).good()) {
        if (!obs.load(persist).ok()) std::printf("coordinator: load failed (fresh)\n");
    }
    obs.advance_epoch();
    std::printf("coordinator: epoch=%llu on 127.0.0.1:%d\n", (unsigned long long)obs.epoch().value(), port);
    std::fflush(stdout);

    SOCKET ls = bind_listen(port);
    if (ls == INVALID_SOCKET) { std::printf("coordinator: bind failed\n"); return 1; }

    for (;;) {
        SOCKET c = ::accept(ls, nullptr, nullptr);
        if (c == INVALID_SOCKET) continue;
        std::thread([c, &obs, &persist]() {
            FramedConnection conn(static_cast<std::uintptr_t>(c));
            SourceId conn_source;
            bool have_source = false;
            for (;;) {
                Frame f;
                bool closed = false;
                if (!conn.recv_frame(f, closed)) break;
                if (f.kind == MsgKind::Hello) {
                    std::size_t off = 0;
                    if (f.body.size() >= 16) {
                        std::uint32_t wid = get_u32(f.body, off);
                        std::uint64_t bid = get_u64(f.body, off);
                        std::uint32_t sg = get_u32(f.body, off);
                        SourceInfo si;
                        si.id = SourceId(wid);
                        si.generation = SourceGeneration(sg);
                        si.worker = WorkerId(wid);
                        si.boot = WorkerBootId(bid);
                        si.name = "worker-" + std::to_string(wid);
                        si.backend = "tcp";
                        obs.register_source(si);
                        conn_source = si.id;
                        have_source = true;
                    }
                    std::vector<std::uint8_t> reply;
                    put_u32(reply, kFrameVersion);
                    put_u64(reply, obs.epoch().value());
                    put_u64(reply, obs.health().accepted);
                    conn.send_frame(MsgKind::HelloReply, reply);
                } else if (f.kind == MsgKind::Observation) {
                    Observation o;
                    std::size_t off = 0;
                    if (decode_observation(f.body.data(), f.body.size(), off, o)) (void)obs.ingest(o);
                } else if (f.kind == MsgKind::Query) {
                    Tick now = clk.now_tick();
                    auto v = obs.device_window(DeviceId(1), Interval{0, now});
                    std::vector<std::uint8_t> pay;
                    put_u64(pay, obs.health().accepted);
                    put_u64(pay, v.compute_busy_time);
                    put_u64(pay, v.useful_compute_time);
                    put_u64(pay, v.reserved_idle_time);
                    put_u64(pay, obs.capacity(DeviceId(1), now).stranded_bytes);
                    put_u64(pay, obs.canonical_digest());
                    put_u32(pay, static_cast<std::uint32_t>(obs.all_sources().size()));
                    char text[512];
                    std::snprintf(text, sizeof(text), "epoch=%llu busy=%llu useful=%llu sources=%u",
                                  (unsigned long long)obs.epoch().value(), (unsigned long long)v.compute_busy_time,
                                  (unsigned long long)v.useful_compute_time,
                                  static_cast<unsigned>(obs.all_sources().size()));
                    pay.insert(pay.end(), text, text + std::strlen(text) + 1);
                    conn.send_frame(MsgKind::Snapshot, pay);
                    { std::lock_guard<std::mutex> lk(g_save); obs.save(persist); }
                } else if (f.kind == MsgKind::Close) {
                    break;
                }
            }
            if (have_source) obs.mark_source_disconnected(conn_source, 0);
            { std::lock_guard<std::mutex> lk(g_save); obs.save(persist); }
        }).detach();
    }
    return 0;
}
