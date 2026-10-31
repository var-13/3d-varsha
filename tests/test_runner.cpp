// tests/test_runner.cpp
// Tiny, zero-dependency test framework.
// Usage: include this file ONCE in one translation unit.
//        Include test_runner.hpp from every test file.
//        Add test cases with TEST("name") { ... } macros.
//        main() lives here and runs all registered tests.

#include "test_runner.hpp"
#include <iostream>
#include <cstdlib>

// Global registry populated by TEST() macros before main().
std::vector<TestCase>& test_registry() {
    static std::vector<TestCase> v;
    return v;
}

int main() {
    int passed = 0, failed = 0;
    std::cout << "=== ThermoBench Test Runner ===\n";

    for (auto& tc : test_registry()) {
        std::cout << "  [ RUN ] " << tc.name << "\n";
        try {
            tc.fn();
            std::cout << "  [ OK  ] " << tc.name << "\n";
            ++passed;
        } catch (const TestFailure& e) {
            std::cout << "  [FAIL ] " << tc.name << " : " << e.what() << "\n";
            ++failed;
        } catch (const std::exception& e) {
            std::cout << "  [FAIL ] " << tc.name << " (exception): " << e.what() << "\n";
            ++failed;
        }
    }

    std::cout << "\nResult: " << passed << " passed, " << failed << " failed.\n";
    return (failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
