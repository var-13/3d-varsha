#pragma once

#include "core/fem1d.hpp"
#include <vector>
#include <string>
#include <chrono>
#include <cstddef>

namespace thermobench {
namespace batch {

struct TestCase {
    int id{0};
    int N{10};
    double L{1.0};
    double A{1e-4};
    double k{10.0};
    double q{10000.0};
    double T0{100.0};
    double TL{20.0};
};

struct CaseResult {
    int id{0};
    thermo::SimulationResult sim;
};

struct BatchResult {
    std::vector<CaseResult> results;
    double elapsed_ms{0.0};
    unsigned int threads_used{1};
};

struct BenchmarkRun {
    unsigned int threads{1};
    std::vector<double> run_times_ms;
    double median_time_ms{0.0};
    double speedup{1.0};
    double efficiency{1.0};
};

struct BenchmarkSummary {
    size_t case_count{0};
    int mesh_N{10};
    std::vector<BenchmarkRun> runs;
    double max_discrepancy_C{0.0};
    bool parity_passed{false};
};

struct BenchmarkConfig {
    size_t case_count{100};
    int mesh_N{40};
    unsigned int seed{42};
    std::vector<unsigned int> thread_counts{1, 2, 4};
    int warmup_runs{1};
    int measured_runs{5};
};

// 1. Deterministic test case generation with varied k, q > 0
std::vector<TestCase> generate_cases(size_t count, unsigned int seed = 42, int N = 40);

// 2. Pure serial solver
BatchResult solve_serial(const std::vector<TestCase>& cases);

// 3. Multithreaded parallel solver using bounded worker threads
BatchResult solve_parallel(const std::vector<TestCase>& cases, unsigned int num_threads);

// 4. Parity comparator: computes max absolute node temperature difference across all cases
double compare_results(const BatchResult& r1, const BatchResult& r2);

// 5. End-to-end benchmark suite: warm-up, 5 measured runs, median time, speedup, efficiency
BenchmarkSummary run_benchmark(const BenchmarkConfig& config);

// 6. CSV exporter for benchmark metrics
std::string to_csv(const BenchmarkSummary& summary);
void export_benchmark_csv(const BenchmarkSummary& summary, const std::string& filepath);

} // namespace batch
} // namespace thermobench
