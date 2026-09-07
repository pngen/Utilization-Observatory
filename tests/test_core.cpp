#include "test_framework.hpp"
#include "uo/arithmetic.hpp"
#include "uo/crc.hpp"
#include "uo/digest.hpp"
#include "uo/ids.hpp"
#include "uo/interval.hpp"
#include "uo/model.hpp"
#include "uo/status.hpp"
#include "uo/time.hpp"

#include <cstring>
#include <string>

using namespace uo;

TEST(basic_id_identity) {
    DeviceId a(7), b(7), c(9);
    CHECK(a == b);
    CHECK(a != c);
    CHECK_EQ(a.value(), 7u);
    CHECK(a.valid());
    CHECK(!DeviceId().valid());
}

TEST(ids_are_not_interchangeable) {
    // Distinct identity domains are distinct C++ types (compile-time safety).
    bool same = std::is_same<DeviceId, WorkloadId>::value;
    CHECK(!same);
    DeviceId d(5);
    WorkloadId w(5);
    CHECK_EQ(d.value(), w.value());  // same opaque value in different domains
}

TEST(checked_add_overflow) {
    std::uint64_t out = 0;
    bool ok = checked_add_u64(UINT64_MAX - 1, 0, out);
    CHECK(ok);
    ok = checked_add_u64(UINT64_MAX - 1, 2, out);
    CHECK(!ok);  // overflow caught
    ok = checked_add_u64(10, 20, out);
    CHECK(ok);
    CHECK_EQ(out, 30u);
}

TEST(checked_mul_overflow) {
    std::uint64_t out = 0;
    bool ok = checked_mul_u64(1ULL << 40, 1ULL << 24, out);  // 2^64 -> overflow
    CHECK(!ok);
    ok = checked_mul_u64(2, 3, out);
    CHECK(ok);
    CHECK_EQ(out, 6u);
}

TEST(interval_exact_intersection) {
    Interval a{10, 50};
    Interval b{30, 80};
    Interval x = intersect(a, b);
    CHECK_EQ(x.begin, 30u);
    CHECK_EQ(x.end, 50u);
    CHECK_EQ(overlap_duration(a, b), 20u);
}

TEST(interval_no_overlap_and_zero_duration) {
    Interval a{10, 20};
    Interval b{20, 30};
    Interval zero{5, 5};
    Interval inv{5, 3};
    CHECK_EQ(overlap_duration(a, b), 0u);
    CHECK(zero.is_empty());
    CHECK_EQ(zero.duration(), 0u);
    CHECK(!inv.valid());  // inverted
}

TEST(crc_deterministic_and_detects_flip) {
    const char* s = "Utilization Observatory";
    std::uint32_t a = crc32c(s, std::strlen(s));
    std::uint32_t b = crc32c(s, std::strlen(s));
    CHECK_EQ(a, b);
    const char* t = "Utilization Observatorx";  // one-bit flip
    std::uint32_t c = crc32c(t, std::strlen(t));
    CHECK(a != c);
}

TEST(digest_deterministic) {
    Digest64 d1, d2;
    d1.put_u64(1); d1.put_u64(2); d1.put_u64(3);
    d2.put_u64(1); d2.put_u64(2); d2.put_u64(3);
    CHECK_EQ(d1.value(), d2.value());
    Digest64 d3; d3.put_u64(1); d3.put_u64(2); d3.put_u64(4);
    CHECK(d1.value() != d3.value());
    Digest64 d4; d4.put_double(0.5);
    Digest64 d5; d5.put_double(0.5);
    CHECK_EQ(d4.value(), d5.value());
}

TEST(clock_scripted) {
    ScriptedClock c(100);
    CHECK_EQ(c.now_tick(), 100u);
    c.advance(50);
    CHECK_EQ(c.now_tick(), 150u);
}

TEST(enum_strings_nonempty) {
    CHECK(std::string(to_string(StateCategory::ReservedIdle)).find("RESERVED") != std::string::npos);
    CHECK(std::string(to_string(Provenance::Measured)).find("MEASURED") != std::string::npos);
    CHECK(std::string(to_string(Freshness::RevalidationRequired)).find("REVALIDATION") != std::string::npos);
    CHECK(std::string(to_string(ObservationType::EfficiencySummary)).find("EFFICIENCY") != std::string::npos);
}

TEST(status_shared_ok) {
    Status s = Status::success();
    CHECK(s.ok());
    CHECK(s.code() == StatusCode::Ok);
    CHECK(Status(StatusCode::InvalidInput).code() == StatusCode::InvalidInput);
    CHECK(!Status(StatusCode::InvalidInput).ok());
}
