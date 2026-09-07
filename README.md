# Utilization Observatory

Utilization Observatory is an open-source, vendor-neutral C++20 observability runtime for explaining where accelerator capacity is actually going across execution, stalls, memory, transfers, residency, retries, fragmentation, reservations, and idle/stranded states.

Its core systems question is:

> Where is accelerator capacity actually going right now, how much of it is producing useful work, what fraction is overhead, waste, waiting, idle, reserved, fragmented, blocked, or otherwise unavailable, and what evidence explains the gap between headline utilization and useful execution?

The thesis is: **utilization is not useful work.** Utilization Observatory reconstructs that difference from evidence. It is not a generic GPU dashboard, not a replacement for nvtop / nvidia-smi / DCGM / ROCm SMI, not an efficiency ledger, and not an interference detector. It is the evidence boundary that explains how nominal accelerator capacity became actual useful execution, overhead, waste, waiting, idle, reserved, stranded, blocked, or unknown state over time.

## Systems boundary

Utilization Observatory owns:

- accelerator-capacity observation;
- utilization evidence ingestion;
- utilization-source identity, provenance, and freshness;
- device busy/idle evidence and execution-active time;
- queue/wait, transfer-active, memory-pressure, recovery, and retry time;
- residency- and reservation-held capacity;
- fragmentation-stranded, authority-fenced, unavailable, and failed/retrying-work visibility;
- device-time and capacity-time reconstruction;
- derived utilization vectors, timeline and interval reconstruction;
- cross-source correlation (conservative, never causal);
- source health and stale-source detection;
- utilization explanations (headline vs useful gap, busy/idle taxonomy);
- persistence, replay, restart, stable digests;
- distributed evidence collection;
- structured UNKNOWN state and structured uncertainty.

Utilization Observatory does **not** own: useful/waste attribution policy, cost optimization, SLO enforcement, interference attribution, contention resolution, scheduling, admission control, quota enforcement, capacity reservation or fragmentation remediation, power control, or vendor-specific monitoring replacement.

### Relationship to Efficiency Ledger

Efficiency Ledger is the previous architectural layer. It owns exact classification of physical work as USEFUL / NECESSARY_OVERHEAD / AVOIDABLE_WASTE / AVOIDED_WORK / STRANDED_CAPACITY / UNKNOWN. Utilization Observatory may ingest Efficiency Ledger summaries (or compatible evidence) via `UsefulWorkSummary` / `EfficiencySummary` observations to explain how headline busy time differs from useful execution. It does not rebuild the ledger.

### Relationship to Interference Observatory

Interference Observatory is the next layer and measures cross-workload interference from shared-resource contention. When evidence exists, Utilization Observatory may report that a utilization change *correlated* with another signal, and labels it `TEMPORALLY_ASSOCIATED` or `POTENTIAL_CONTRIBUTOR`. It never claims causal interference. It does not absorb Interference Observatory scope.

## Physical vs logical utilization

Physical device capacity (exists, published, available, allocated, reserved, resident, fit-qualified, stranded, blocked, unknown) is kept distinct from logical workload state and from compute/memory utilization. A device can report 0% compute while 28 GiB of VRAM is intentionally resident, or report large free memory while fragmentation prevents the next fit. Both are shown as distinct dimensions.

## Headline vs useful utilization

This is a primary feature. Given device utilization plus compatible Efficiency-Ledger evidence, `gap()` and `explain()` compute headline busy, useful execution, non-useful busy, overhead, idle, reserved-idle, stranded-capacity, and unknown ratios. Every ratio is surfaced as a numerator/denominator (`Ratio`); with no denominator the ratio is UNKNOWN, never a fabricated zero. Useful-work attribution is never invented when Efficiency-Ledger-compatible evidence is absent.

## Busy / idle taxonomy

Busy time is classified (where evidence exists) as USEFUL, NECESSARY_OVERHEAD, AVOIDABLE_WASTE, RETRY, RECOMPUTATION, RECOVERY, TRANSFER_ASSIST, or UNKNOWN_BUSY. Idle capacity is classified as AVAILABLE_IDLE, RESERVED_IDLE, RESIDENCY_IDLE, DEPENDENCY_BLOCKED, AUTHORITY_FENCED, FRAGMENTATION_STRANDED, CAPACITY_UNFIT, WORKLOAD_STARVED, RECOVERY_WAIT, or UNKNOWN_IDLE. Reserved idle is never merged into available idle; stranded capacity is never merged into consumed capacity.

## Reservation / residency / fragmentation treatment

The runtime accepts Reservation Fabric-like evidence (`ReservationBegin/End`), Engine/Model Residency-like evidence (`ResidencyBegin/End`), and Fragmentation Governor-like evidence (`FragmentationObserved/Cleared`) and reports their utilization consequences. It does not decide reservation or residency policy and does not remediate fragmentation.

## Source provenance and freshness

Every observation carries explicit provenance (MEASURED, REPORTED, DERIVED, RECONSTRUCTED, ESTIMATED, SYNTHETIC, POLICY, UNKNOWN), an evidence label (REAL / DERIVED / SYNTHETIC / UNSUPPORTED), a `SampleSemantics` (instantaneous, averaged prior interval, rolling window, backend-opaque), and a generation. Freshness is tracked per source (CURRENT, STALE, EXPIRED, REVALIDATION_REQUIRED, HISTORICAL, UNKNOWN); a missing heartbeat never silently preserves current state indefinitely. Source health (HEALTHY, DEGRADED, STALE, DISCONNECTED, REVALIDATION_REQUIRED, UNSUPPORTED, UNKNOWN) and last-observation/wall-clock are exposed.

## Backend capability model

Capabilities are independently queryable per device: compute utilization, memory utilization, memory bytes, power, temperature, clocks, per-process utilization, engine utilization, kernel intervals, transfer intervals, NVLink, RDMA, MIG. Unsupported capability is `UNSUPPORTED`, never zero.

## Timeline, snapshots, and gap analysis

`timeline()` returns a deterministic merged multi-source event list. `snapshot()` returns a coherent device snapshot with an explicit consistency flag and freshness. `gap()` / `explain()` return the headline-vs-useful gap and ranked contributors with provenance. Correlation is conservative: `TEMPORALLY_ASSOCIATED`, `POTENTIAL_CONTRIBUTOR`, `UNKNOWN_CAUSE`.

## Authority and generation model

Strong identities (`CoordinatorEpoch`, `SourceId/Generation`, `WorkerId/WorkerBootId`, `DeviceId/Generation`, `WorkloadId/Generation`, `EvidenceId/Generation`, and others) are opaque typed values. Old coordinator epochs, stale source generations, stale worker boots, stale device/workload/evidence generations, and duplicate observation ids are rejected. A disconnected source may re-establish authority with a fresh boot; after a coordinator restart, dynamic source state requires revalidation.

## Persistence, replay, and digest

Persistence is versioned with a fixed format, CRC-32C record integrity, bounded decode, checked arithmetic, and atomic save/replace. Corruption, truncation, trailing garbage, and unknown versions are rejected as `PERSISTENCE_CORRUPT`. Replay reconstructs identical historical timelines and totals. `canonical_digest()` is a deterministic FNV-1a 64-bit digest over the canonical observation stream (excludes memory addresses, unordered iteration, temporary names, wall-clock noise, and ephemeral handles).

## Distributed proof

A framed TCP protocol (version, length bound, checksum, partial-read/partial-write safety, malformed and oversize rejection) carries observations from real worker OS processes to a real coordinator process over loopback. The distributed proof validates active+idle workers, retry/useful gap, real worker death (the source becomes disconnected and stale-boot replay is rejected; a fresh boot re-establishes authority), reserved/stranded distinction, a real coordinator restart (historical timeline survives, dynamic state requires revalidation, old-epoch traffic rejects), and duplicate/stale rejection.

## Real CUDA/NVML proof

On an RTX 5090 (sm_120) with CUDA and NVML, `uo_cuda_proof` runs real hardware paths: (A) useful busy work (real allocation, H2D, kernel loop, D2H, CPU parity); (B) busy-but-non-useful (rejected before publication); (C) transfer-heavy H2D/D2H with events; (D) idle residency (VRAM held, low/absent compute); (E) retry then success; (F) fragmentation derived from real allocation geometry. Device memory returns to baseline within driver overhead.

## REAL / DERIVED / SYNTHETIC / UNSUPPORTED

Real evidence is actual CUDA allocation, kernel, transfer, NVML sample, OS process, or TCP. Derived evidence is interval overlap, ratios, byte-time, timeline reconstruction, and gap analysis. Synthetic evidence is explicitly labeled fixtures (e.g., fragmentation geometry that cannot be deterministically created on hardware). Unsupported telemetry remains `UNSUPPORTED`.

## Build

Requires C++20 and CMake 3.20+. On Windows, MSVC with Ninja is used; strict warnings (`/W4 /WX`) are enforced for the library and all targets. `UO_ENABLE_CUDA` (default OFF) builds the optional CUDA/NVML proof; it is not required for the core.

## Install

`cmake --build build`, then `cmake --install build --prefix <prefix>` installs `utilization_observatory` and exports the `UtilizationObservatory` CMake package. A downstream consumer links against `UtilizationObservatory::UtilizationObservatory` via `find_package(UtilizationObservatory CONFIG REQUIRED)`.

## Examples

Separate runnable programs cover basic device utilization, headline-vs-useful gap, retry gap, idle reservation, idle residency, fragmentation-stranded capacity, transfer-heavy workload, worker-loss/revalidation, historical replay, multi-source timeline, unsupported telemetry, and unknown source state (`uo_examples`).

## Limitations

- The core is interval-reconstruction based; an open interval (BEGIN without END) contributes no attributed duration until it is closed, to avoid fabricating extent.
- Per-workload useful-work attribution requires Efficiency-Ledger-compatible evidence; without it, useful busy is reported as UNKNOWN rather than zero.
- Window aggregation scans the retained per-device event list (O(N)); a persistent index is a future concern, correctness first.
- The CUDA/NVML proof requires a compatible NVIDIA device; on hosts without one it reports `SKIP`.

## License

Apache License 2.0. Copyright 2026 Summon Software Labs. No telemetry transmission.
