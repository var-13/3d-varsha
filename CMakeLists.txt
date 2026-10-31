cmake_minimum_required(VERSION 3.20)
project(ThermoBench VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── Output directories ──────────────────────────────────────────────────────
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# ── Compiler warnings ───────────────────────────────────────────────────────
if(MSVC)
    add_compile_options(/W4 /WX)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()

# ── Optimisation flags (Release) ────────────────────────────────────────────
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    if(NOT MSVC)
        add_compile_options(-O2)
    endif()
endif()

# ── Include paths ───────────────────────────────────────────────────────────
include_directories(${CMAKE_SOURCE_DIR}/src)
# vendor/ is treated as a SYSTEM directory so -Werror/-Wall don't fire inside httplib.h
include_directories(SYSTEM ${CMAKE_SOURCE_DIR}/vendor)

# ── Core library (pure numerics, no UI/network) ─────────────────────────────
add_library(thermo_core STATIC
    src/core/fem1d.cpp
    src/core/backprop.cpp
    src/core/batch_solver.cpp
    src/core/geometry.cpp
    src/core/embedded_mock.cpp
)
target_include_directories(thermo_core PUBLIC src)

# ── HTTP server ─────────────────────────────────────────────────────────────
# Disable SSL in httplib – we only need plain HTTP on localhost.
# httplib uses #ifdef CPPHTTPLIB_OPENSSL_SUPPORT so we must NOT define it at all.
# (Setting =0 would still satisfy #ifdef and pull in SSL symbols.)
# No action needed: undefined = SSL off.
add_executable(ThermoBenchServer
    src/server/main.cpp
)
target_link_libraries(ThermoBenchServer PRIVATE thermo_core)
if(WIN32)
    target_link_libraries(ThermoBenchServer PRIVATE ws2_32)
endif()


# ── Launcher ────────────────────────────────────────────────────────────────
add_executable(Launch
    src/launcher/launch.cpp
)
if(WIN32)
    target_link_libraries(Launch PRIVATE ws2_32)
endif()

# ── Test runner ─────────────────────────────────────────────────────────────
add_executable(RunTests
    tests/test_runner.cpp
    tests/test_phase0.cpp
    tests/test_phase1.cpp
    tests/test_phase2.cpp
    tests/test_phase3.cpp
    tests/test_phase4.cpp
    tests/test_phase5.cpp
)
target_link_libraries(RunTests PRIVATE thermo_core)

enable_testing()
add_test(NAME AllTests COMMAND RunTests)
