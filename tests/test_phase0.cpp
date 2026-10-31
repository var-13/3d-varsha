// tests/test_phase0.cpp
// Phase 0 tests: verify the test framework itself and the version string.

#include "test_runner.hpp"
#include "core/fem1d.hpp"
#include <string>

// ── Test 1: framework sanity – a trivially true assertion ────────────────────
TEST("framework: ASSERT true") {
    ASSERT(1 + 1 == 2);
}

// ── Test 2: framework – ASSERT_NEAR within tolerance ────────────────────────
TEST("framework: ASSERT_NEAR passes") {
    ASSERT_NEAR(3.14159, 3.14160, 1e-4);
}

// ── Test 3: framework – ASSERT_NEAR fails when difference is too large ───────
TEST("framework: ASSERT_NEAR fails correctly") {
    bool caught = false;
    try {
        ASSERT_NEAR(0.0, 1.0, 1e-10);  // must throw
    } catch (const TestFailure&) {
        caught = true;
    }
    ASSERT(caught);  // we expect it threw
}

// ── Test 4: framework – ASSERT_THROWS works ──────────────────────────────────
TEST("framework: ASSERT_THROWS passes") {
    ASSERT_THROWS(throw std::runtime_error("ok"));
}

// ── Test 5: version string is not empty ──────────────────────────────────────
TEST("core: version() returns non-empty string") {
    std::string v = thermo::version();
    ASSERT(!v.empty());
}

// ── Test 6: version string contains "ThermoBench" ────────────────────────────
TEST("core: version() contains 'ThermoBench'") {
    std::string v = thermo::version();
    ASSERT(v.find("ThermoBench") != std::string::npos);
}
