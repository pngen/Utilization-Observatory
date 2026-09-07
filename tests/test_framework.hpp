#pragma once
// Minimal self-contained test framework. No external dependencies, no test
// timeouts, no watchdog logic: a hang is a defect.
#include <cmath>
#include <cstdio>
#include <exception>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace test {
inline std::vector<std::pair<std::string, void (*)()>>& registry() {
    static std::vector<std::pair<std::string, void (*)()>> r;
    return r;
}
struct Registrar {
    Registrar(const std::string& n, void (*f)()) { registry().push_back({n, f}); }
};
inline int& failures() { static int f = 0; return f; }
inline void check(bool c, const char* expr, const char* file, int line) {
    if (!c) {
        ++failures();
        std::printf("  FAIL %s:%d: %s\n", file, line, expr);
    }
}
inline int run_all() {
    int total = 0, failed = 0;
    for (auto& [name, f] : registry()) {
        failures() = 0;
        std::printf("[ RUN ] %s\n", name.c_str());
        try { f(); }
        catch (const std::exception& e) {
            ++failures();
            std::printf("  EXCEPTION: %s\n", e.what());
        }
        if (failures() == 0) std::printf("[ OK  ] %s\n", name.c_str());
        else {
            std::printf("[ FAIL ] %s (%d checks)\n", name.c_str(), failures());
            ++failed;
        }
        ++total;
    }
    std::printf("==== %d tests, %d failed ====\n", total, failed);
    return failed == 0 ? 0 : 1;
}
} // namespace test

#define TEST(name) static void test_##name(); \
    static test::Registrar reg_##name(#name, &test_##name); \
    static void test_##name()
#define CHECK(...) test::check(static_cast<bool>(__VA_ARGS__), #__VA_ARGS__, __FILE__, __LINE__)
#define CHECK_EQ(a, b) test::check(static_cast<bool>((a) == (b)), #a " == " #b, __FILE__, __LINE__)
#define CHECK_NEAR(a, b, e) test::check(std::fabs((a) - (b)) <= (e), #a " ~ " #b, __FILE__, __LINE__)
