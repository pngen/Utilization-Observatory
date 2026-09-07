// Utilization Observatory : distributed proof runner (real OS processes + TCP).
// Spawns a coordinator and workers, drives scenarios, verifies via queries and by
// sending observations as a client. Copyright 2026 Summon Software Labs. Apache-2.0.
#include "protocol.hpp"
#include "uo/codec.hpp"
#include "uo/model.hpp"
#include "uo/observation.hpp"
#include "uo/time.hpp"
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

using namespace uo;
using namespace uo::dist;

static void sleep_ms(int ms) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }

static HANDLE run_process(const std::string& exe, const std::string& args) {
    std::string cmd = "\"" + exe + "\" " + args;
    STARTUPINFOA si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    std::vector<char> buf(cmd.begin(), cmd.end());
    buf.push_back('\0');
    if (!CreateProcessA(nullptr, buf.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
        std::printf("spawn failed: %s\n", cmd.c_str());
        return nullptr;
    }
    CloseHandle(pi.hThread);
    return pi.hProcess;
}

static SOCKET connect_to(int port) {
    SOCKET s = ::socket(AF_INET, SOCK_STREAM, 0);
    if (s == INVALID_SOCKET) return INVALID_SOCKET;
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(static_cast<u_short>(port));
    if (::connect(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) { ::closesocket(s); return INVALID_SOCKET; }
    return s;
}

static bool send_observation(int port, const Observation& o) {
    SOCKET s = connect_to(port);
    if (s == INVALID_SOCKET) return false;
    FramedConnection conn((std::uintptr_t)s);
    std::vector<std::uint8_t> pay;
    encode_observation(pay, o);
    bool ok = conn.send_frame(MsgKind::Observation, pay);
    conn.send_frame(MsgKind::Close, {});
    return ok;
}

static bool query_snapshot(int port, SnapshotPayload& sp) {
    SOCKET s = connect_to(port);
    if (s == INVALID_SOCKET) return false;
    FramedConnection conn((std::uintptr_t)s);
    if (!conn.send_frame(MsgKind::Query, {})) return false;
    Frame f;
    bool closed = false;
    if (!conn.recv_frame(f, closed) || f.kind != MsgKind::Snapshot || f.body.size() < 52) return false;
    std::size_t off = 0;
    sp.accepted = get_u64(f.body, off);
    sp.busy_ns = get_u64(f.body, off);
    sp.useful_ns = get_u64(f.body, off);
    sp.reserved_ns = get_u64(f.body, off);
    sp.stranded_ns = get_u64(f.body, off);
    sp.digest = get_u64(f.body, off);
    sp.sources = get_u32(f.body, off);
    std::memcpy(sp.text, f.body.data() + off, std::min(f.body.size() - off, sizeof(sp.text) - 1));
    sp.text[sizeof(sp.text) - 1] = '\0';
    conn.send_frame(MsgKind::Close, {});
    return true;
}

static Observation make_client_obs(ObservationId id, CoordinatorEpoch epoch, std::uint32_t wid,
                                   std::uint64_t boot, StateCategory cat) {
    Observation o;
    o.id = id; o.coordinator_epoch = epoch; o.type = ObservationType::DeviceSample;
    o.begin = 90000; o.end = 90000;
    o.source = SourceId(wid); o.source_generation = SourceGeneration(1);
    o.worker = WorkerId(wid); o.worker_boot = WorkerBootId(boot);
    o.device = DeviceId(1); o.device_generation = DeviceGeneration(1);
    o.state = cat;
    o.percent = 10.0;
    return o;
}

int main(int argc, char** argv) {
    setvbuf(stdout, nullptr, _IONBF, 0);
    std::printf("proof: start\n");
    WSADATA w; WSAStartup(MAKEWORD(2, 2), &w);
    if (argc < 6) { std::printf("usage: uo_distributed_proof <coordinator> <worker> <port> <persist> <mode>\n"); return 1; }
    const std::string coord = argv[1], wk = argv[2];
    int port = std::atoi(argv[3]);
    const std::string persist = argv[4];
    const std::string mode = argv[5];

    int pass = 0, fail = 0;
    auto report = [&](const char* name, bool ok) { std::printf("Scenario %-14s: %s\n", name, ok ? "PASS" : "FAIL"); std::fflush(stdout); (ok ? pass : fail)++; };

    // Start coordinator (incarnation 1).
    HANDLE coord_h = run_process(coord, std::to_string(port) + " \"" + persist + "\"");
    sleep_ms(800);

    SnapshotPayload sp;
    bool coord_up = query_snapshot(port, sp);
    report("coordinator up", coord_up);

    if (mode == "A" || mode == "all") {
        HANDLE wa = run_process(wk, std::to_string(port) + " 11 111 active 1");
        HANDLE wb = run_process(wk, std::to_string(port) + " 22 222 idle 1");
        sleep_ms(1200);
        SnapshotPayload sp2;
        if (query_snapshot(port, sp2)) {
            bool ok = sp2.accepted >= 2 && sp2.busy_ns > 0 && sp2.useful_ns > 0 && sp2.sources >= 1;
            report("A active+idle", ok);
        } else report("A active+idle", false);
        (void)wa; (void)wb;
    }

    if (mode == "C" || mode == "all") {
        HANDLE wc = run_process(wk, std::to_string(port) + " 33 333 active 1");
        sleep_ms(800);
        SnapshotPayload before;
        query_snapshot(port, before);
        // Kill worker C (real OS process death).
        TerminateProcess(wc, 0);
        sleep_ms(800);
        // Fresh-boot worker C' reconnects.
        HANDLE wc2 = run_process(wk, std::to_string(port) + " 33 444 active 1");
        sleep_ms(800);
        // Stale-boot replay (old boot 333) must be rejected: accepted unchanged.
        SnapshotPayload s1; query_snapshot(port, s1);
        send_observation(port, make_client_obs(ObservationId(7777), CoordinatorEpoch(2), 33, 333, StateCategory::UsefulExecution));
        sleep_ms(300);
        SnapshotPayload s2; query_snapshot(port, s2);
        report("C worker-death stale-reject", s2.accepted == s1.accepted);
        // Duplicate rejection: accepted unchanged after resending a known id.
        send_observation(port, make_client_obs(ObservationId(1), CoordinatorEpoch(2), 33, 444, StateCategory::UsefulExecution));
        SnapshotPayload s3; query_snapshot(port, s3);
        report("C duplicate reject", s3.accepted == s2.accepted);
        (void)wc2;
    }

    if (mode == "D" || mode == "all") {
        HANDLE wr = run_process(wk, std::to_string(port) + " 44 444 reserved 0");
        HANDLE ws = run_process(wk, std::to_string(port) + " 55 555 stranded 0");
        sleep_ms(1500);
        SnapshotPayload sd; query_snapshot(port, sd);
        std::printf("  D reserved_ns=%llu stranded_ns=%llu\n", (unsigned long long)sd.reserved_ns, (unsigned long long)sd.stranded_ns);
        report("D reserved distinct", sd.reserved_ns > 0);
        report("D stranded distinct", sd.stranded_ns > 0);
        (void)wr; (void)ws;
    }

    if (mode == "E" || mode == "all") {
        // Coordinator restart.
        TerminateProcess(coord_h, 0);
        sleep_ms(1000);
        HANDLE coord2 = run_process(coord, std::to_string(port) + " \"" + persist + "\"");
        sleep_ms(1200);
        // Historical summary survives the restart (loaded from persistence).
        SnapshotPayload sh; bool up = query_snapshot(port, sh);
        report("E restart up", up);
        report("E history survives", up && sh.busy_ns > 0);
        // Old-epoch traffic (epoch 1) must reject: accepted unchanged.
        SnapshotPayload e1; query_snapshot(port, e1);
        send_observation(port, make_client_obs(ObservationId(8888), CoordinatorEpoch(1), 33, 333, StateCategory::UsefulExecution));
        sleep_ms(300);
        SnapshotPayload e2; query_snapshot(port, e2);
        report("E old-epoch reject", e2.accepted == e1.accepted);
        coord_h = coord2;
    }

    if (mode == "F" || mode == "all") {
        SnapshotPayload sf; query_snapshot(port, sf);
        report("F snapshot/digest", sf.digest != 0);
    }

    std::printf("=== Distributed result: %d pass, %d fail ===\n", pass, fail);
    TerminateProcess(coord_h, 0);
    return fail == 0 ? 0 : 1;
}
