// Utilization Observatory : distributed worker (real OS process, TCP).
// Sends Hello, learns the coordinator epoch, then publishes observations for a
// scenario and (optionally) stays connected so the coordinator can observe its
// death as a real source loss. Copyright 2026 Summon Software Labs. Apache-2.0.
#include "protocol.hpp"
#include "uo/codec.hpp"
#include "uo/model.hpp"
#include "uo/observation.hpp"
#include "uo/time.hpp"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

using namespace uo;
using namespace uo::dist;

static Tick g_t = 0;
static Tick tick(Tick delta) { Tick p = g_t; g_t += delta; return p; }

static bool connect_to(int port, SOCKET& out) {
    SOCKET s = ::socket(AF_INET, SOCK_STREAM, 0);
    if (s == INVALID_SOCKET) return false;
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(static_cast<u_short>(port));
    if (::connect(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
        ::closesocket(s);
        return false;
    }
    out = s;
    return true;
}

int main(int argc, char** argv) {
    if (argc < 6) { std::printf("usage: uo_worker <port> <worker_id> <boot_id> <mode> <stay_alive>\n"); return 1; }
    int port = std::atoi(argv[1]);
    long wid = std::atol(argv[2]);
    long boot = std::atol(argv[3]);
    std::string mode = argv[4];
    bool stay = std::atoi(argv[5]) != 0;

    WSADATA w; WSAStartup(MAKEWORD(2, 2), &w);

    SOCKET s = INVALID_SOCKET;
    if (!connect_to(port, s)) { std::printf("worker: connect failed\n"); return 1; }
    FramedConnection conn(static_cast<std::uintptr_t>(s));

    // Hello.
    std::vector<std::uint8_t> hello;
    put_u32(hello, static_cast<std::uint32_t>(wid));
    put_u64(hello, static_cast<std::uint64_t>(boot));
    put_u32(hello, 1u);
    if (!conn.send_frame(MsgKind::Hello, hello)) { std::printf("worker: hello send failed\n"); return 1; }

    // Learn epoch from HelloReply.
    CoordinatorEpoch epoch(1);
    {
        Frame reply; bool closed = false;
        if (conn.recv_frame(reply, closed) && reply.kind == MsgKind::HelloReply && reply.body.size() >= 12) {
            std::size_t off = 0;
            (void)get_u32(reply.body, off);
            epoch = CoordinatorEpoch(get_u64(reply.body, off));
        }
    }

    long long idbase = static_cast<long long>(wid) * 1000000;
    long long counter = 0;
    auto next_obs_id = [&]() -> ObservationId { return ObservationId(static_cast<std::uint64_t>(idbase + counter++)); };
    auto send_obs = [&](ObservationType type, Tick b, Tick e, StateCategory cat) {
        Observation o;
        o.id = next_obs_id();
        o.coordinator_epoch = epoch;
        o.type = type;
        o.begin = b; o.end = e;
        o.source = SourceId(static_cast<std::uint64_t>(wid));
        o.source_generation = SourceGeneration(1);
        o.worker = WorkerId(static_cast<std::uint64_t>(wid));
        o.worker_boot = WorkerBootId(static_cast<std::uint64_t>(boot));
        o.device = DeviceId(1);
        o.device_generation = DeviceGeneration(1);
        o.state = cat;
        std::vector<std::uint8_t> pay;
        encode_observation(pay, o);
        conn.send_frame(MsgKind::Observation, pay);
    };

    if (mode == "active") {
        for (int i = 0; i < 20; ++i) {
            Tick b = tick(1000);
            send_obs(ObservationType::ExecutionBegin, b, b, StateCategory::UsefulExecution);
            send_obs(ObservationType::ExecutionEnd, b + 500, b + 500, StateCategory::UsefulExecution);
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    } else if (mode == "idle") {
        Tick b = tick(1000);
        Observation o;
        o.id = next_obs_id(); o.coordinator_epoch = epoch; o.type = ObservationType::DeviceSample;
        o.begin = b; o.end = b; o.source = SourceId((std::uint64_t)wid); o.source_generation = SourceGeneration(1);
        o.worker = WorkerId((std::uint64_t)wid); o.worker_boot = WorkerBootId((std::uint64_t)boot);
        o.device = DeviceId(1); o.device_generation = DeviceGeneration(1);
        o.percent = 5.0;
        std::vector<std::uint8_t> pay; encode_observation(pay, o);
        conn.send_frame(MsgKind::Observation, pay);
    } else if (mode == "retry") {
        for (int i = 0; i < 5; ++i) {
            Tick b = tick(1000);
            send_obs(ObservationType::ExecutionBegin, b, b, StateCategory::RetryExecution);
            send_obs(ObservationType::ExecutionEnd, b + 400, b + 400, StateCategory::RetryExecution);
        }
        for (int i = 0; i < 5; ++i) {
            Tick b = tick(1000);
            send_obs(ObservationType::ExecutionBegin, b, b, StateCategory::UsefulExecution);
            send_obs(ObservationType::ExecutionEnd, b + 400, b + 400, StateCategory::UsefulExecution);
        }
    } else if (mode == "reserved") {
        Tick b = tick(1000);
        send_obs(ObservationType::ReservationBegin, b, b, StateCategory::ReservedIdle);
        send_obs(ObservationType::ReservationEnd, b + 5000, b + 5000, StateCategory::ReservedIdle);
    } else if (mode == "stranded") {
        Tick b = tick(1000);
        Observation o;
        o.id = next_obs_id(); o.coordinator_epoch = epoch; o.type = ObservationType::FragmentationObserved;
        o.begin = b; o.end = b; o.source = SourceId((std::uint64_t)wid); o.source_generation = SourceGeneration(1);
        o.worker = WorkerId((std::uint64_t)wid); o.worker_boot = WorkerBootId((std::uint64_t)boot);
        o.device = DeviceId(1); o.device_generation = DeviceGeneration(1);
        o.bytes = 12ull << 30; o.capacity_bytes = 32607ull << 20; o.state = StateCategory::FragmentationStranded;
        std::vector<std::uint8_t> pay; encode_observation(pay, o);
        conn.send_frame(MsgKind::Observation, pay);
    } else if (mode == "once") {
        // send one useful interval then close
        Tick b = tick(1000);
        send_obs(ObservationType::ExecutionBegin, b, b, StateCategory::UsefulExecution);
        send_obs(ObservationType::ExecutionEnd, b + 1000, b + 1000, StateCategory::UsefulExecution);
        conn.send_frame(MsgKind::Close, {});
        return 0;
    }

    if (stay) std::this_thread::sleep_for(std::chrono::seconds(60));
    else conn.send_frame(MsgKind::Close, {});
    return 0;
}
