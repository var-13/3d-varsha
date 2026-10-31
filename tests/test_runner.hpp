// tests/test_runner.hpp
// Lightweight test framework – header only (no Catch2, no GTest).
//
// How it works:
//   1. TEST("my test") { ... }  expands to a struct whose constructor
//      registers the lambda in a global vector.
//   2. test_runner.cpp defines main() which iterates the vector.
//   3. ASSERT(expr) throws TestFailure if expr is false.
//   4. ASSERT_NEAR(a, b, tol) checks |a-b| <= tol.

#pragma once
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>
#include <cmath>
#include <sstream>

// ── Exception thrown when an assertion fails ─────────────────────────────────
struct TestFailure : std::runtime_error {
    explicit TestFailure(const std::string& msg) : std::runtime_error(msg) {}
};

// ── A single named test case ─────────────────────────────────────────────────
struct TestCase {
    std::string name;
    std::function<void()> fn;
};

// Declared in test_runner.cpp; returns the global registry.
std::vector<TestCase>& test_registry();

// ── Assertion macros ─────────────────────────────────────────────────────────
#define ASSERT(expr)                                                         \
    do {                                                                     \
        if (!(expr)) {                                                       \
            std::ostringstream _ss;                                          \
            _ss << "ASSERT failed: " #expr                                   \
                << "  (" __FILE__ ":" << __LINE__ << ")";                   \
            throw TestFailure(_ss.str());                                    \
        }                                                                    \
    } while (false)

#define ASSERT_NEAR(a, b, tol)                                               \
    do {                                                                     \
        double _a = static_cast<double>(a);                                  \
        double _b = static_cast<double>(b);                                  \
        double _t = static_cast<double>(tol);                                \
        if (std::abs(_a - _b) > _t) {                                       \
            std::ostringstream _ss;                                          \
            _ss << "ASSERT_NEAR failed: |" << _a << " - " << _b            \
                << "| = " << std::abs(_a-_b) << " > tol=" << _t            \
                << "  (" __FILE__ ":" << __LINE__ << ")";                   \
            throw TestFailure(_ss.str());                                    \
        }                                                                    \
    } while (false)

#define ASSERT_THROWS(expr)                                                  \
    do {                                                                     \
        bool _threw = false;                                                 \
        try { (expr); } catch (...) { _threw = true; }                      \
        if (!_threw) {                                                       \
            std::ostringstream _ss;                                          \
            _ss << "ASSERT_THROWS failed: no exception thrown for: " #expr  \
                << "  (" __FILE__ ":" << __LINE__ << ")";                   \
            throw TestFailure(_ss.str());                                    \
        }                                                                    \
    } while (false)

// ── TEST macro ───────────────────────────────────────────────────────────────
// __COUNTER__ increments once per expansion. We need one unique number N shared
// across the struct definition, variable declaration, and method definition.
// Solution: outer macro captures __COUNTER__ into N, inner macro uses N three times.
#define TEST_CONCAT_(a, b) a##b
#define TEST_CONCAT(a, b)  TEST_CONCAT_(a, b)

#define TEST_IMPL(name_str, N)                                                     \
    namespace {                                                                    \
        struct TEST_CONCAT(TestReg_, N) {                                          \
            TEST_CONCAT(TestReg_, N)() {                                           \
                test_registry().push_back({name_str, [this](){ run(); }});        \
            }                                                                      \
            void run();                                                            \
        } TEST_CONCAT(test_inst_, N);                                              \
    }                                                                              \
    inline void TEST_CONCAT(TestReg_, N)::run()

// Public macro: captures __COUNTER__ once.
#define TEST(name_str) TEST_IMPL(name_str, __COUNTER__)
