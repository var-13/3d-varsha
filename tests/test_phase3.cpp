#include "test_runner.hpp"
#include "core/batch_solver.hpp"
#include <cmath>
#include <fstream>
#include <string>

using namespace thermobench;

namespace {

TEST("Phase 3 - generate_cases: deterministic output for same seed, varied k, q > 0") {
    auto c1 = batch::generate_cases(50, 12345, 20);
    auto c2 = batch::generate_cases(50, 12345, 20);
    ASSERT(c1.size() == 50);
    ASSERT(c2.size() == 50);
    for (size_t i = 0; i < 50; ++i) {
        ASSERT(c1[i].id == static_cast<int>(i));
        ASSERT_NEAR(c1[i].k, c2[i].k, 1e-12);
        ASSERT_NEAR(c1[i].q, c2[i].q, 1e-12);
        ASSERT(c1[i].k >= 5.0 && c1[i].k <= 50.0);
        ASSERT(c1[i].q >= 1000.0 && c1[i].q <= 20000.0);
    }
}

TEST("Phase 3 - generate_cases: zero count or N < 2 throws invalid_argument") {
    ASSERT_THROWS(batch::generate_cases(0, 42, 20));
    ASSERT_THROWS(batch::generate_cases(10, 42, 1));
}

TEST("Phase 3 - solve_serial: solves batch accurately, boundaries match T0 and TL") {
    auto cases = batch::generate_cases(20, 999, 10);
    auto res = batch::solve_serial(cases);
    ASSERT(res.results.size() == 20);
    ASSERT(res.threads_used == 1);
    ASSERT(res.elapsed_ms >= 0.0);
    for (size_t i = 0; i < res.results.size(); ++i) {
        ASSERT(res.results[i].id == static_cast<int>(i));
        ASSERT(res.results[i].sim.T_fe.size() == 11);
        ASSERT_NEAR(res.results[i].sim.T_fe.front(), cases[i].T0, 1e-10);
        ASSERT_NEAR(res.results[i].sim.T_fe.back(), cases[i].TL, 1e-10);
    }
}

TEST("Phase 3 - solve_serial: empty test cases throws invalid_argument") {
    std::vector<batch::TestCase> empty;
    ASSERT_THROWS(batch::solve_serial(empty));
}

TEST("Phase 3 - solve_parallel: numerical parity across 1, 2, 4 threads within 1e-8 C") {
    auto cases = batch::generate_cases(60, 777, 20);
    auto res_serial = batch::solve_serial(cases);
    auto res_p2 = batch::solve_parallel(cases, 2);
    auto res_p4 = batch::solve_parallel(cases, 4);

    ASSERT(res_p2.results.size() == 60);
    ASSERT(res_p4.results.size() == 60);

    double diff2 = batch::compare_results(res_serial, res_p2);
    double diff4 = batch::compare_results(res_serial, res_p4);

    ASSERT(diff2 <= 1e-8);
    ASSERT(diff4 <= 1e-8);
}

TEST("Phase 3 - solve_parallel: zero threads or empty list throws invalid_argument") {
    auto cases = batch::generate_cases(5, 42, 10);
    ASSERT_THROWS(batch::solve_parallel(cases, 0));
    std::vector<batch::TestCase> empty;
    ASSERT_THROWS(batch::solve_parallel(empty, 2));
}

TEST("Phase 3 - compare_results: zero diff for identical, detects perturbations") {
    auto cases = batch::generate_cases(10, 101, 10);
    auto r1 = batch::solve_serial(cases);
    auto r2 = batch::solve_serial(cases);
    ASSERT_NEAR(batch::compare_results(r1, r2), 0.0, 1e-14);

    // Perturb one value
    r2.results[3].sim.T_fe[4] += 0.05;
    ASSERT_NEAR(batch::compare_results(r1, r2), 0.05, 1e-10);

    // Size mismatch throws
    cases.pop_back();
    auto r3 = batch::solve_serial(cases);
    ASSERT_THROWS(batch::compare_results(r1, r3));
}

TEST("Phase 3 - benchmark and CSV export: verified warm-up, 5 runs median, speedup, CSV") {
    batch::BenchmarkConfig cfg;
    cfg.case_count = 40;
    cfg.mesh_N = 20;
    cfg.seed = 42;
    cfg.thread_counts = {1, 2};
    cfg.warmup_runs = 1;
    cfg.measured_runs = 3;

    auto summary = batch::run_benchmark(cfg);
    ASSERT(summary.case_count == 40);
    ASSERT(summary.mesh_N == 20);
    ASSERT(summary.parity_passed);
    ASSERT(summary.max_discrepancy_C <= 1e-8);
    ASSERT(summary.runs.size() == 2);
    ASSERT(summary.runs[0].threads == 1);
    ASSERT(summary.runs[0].speedup == 1.0);
    ASSERT(summary.runs[1].threads == 2);
    ASSERT(summary.runs[1].speedup > 0.0);

    std::string csv = batch::to_csv(summary);
    ASSERT(!csv.empty());
    ASSERT(csv.find("threads,cases,mesh_N") != std::string::npos);

    const std::string test_csv_path = "results/test_benchmark.csv";
    batch::export_benchmark_csv(summary, test_csv_path);
    std::ifstream ifs(test_csv_path);
    ASSERT(ifs.is_open());
    ifs.close();
}

TEST("Phase 3 - generate official benchmark_multicore.csv") {
    batch::BenchmarkConfig cfg;
    cfg.case_count = 200;
    cfg.mesh_N = 40;
    cfg.seed = 42;
    cfg.thread_counts = {1, 2, 4};
    cfg.warmup_runs = 1;
    cfg.measured_runs = 5;

    auto summary = batch::run_benchmark(cfg);
    ASSERT(summary.parity_passed);
    ASSERT(summary.max_discrepancy_C <= 1e-8);

    const std::string official_path = "results/benchmark_multicore.csv";
    batch::export_benchmark_csv(summary, official_path);
    std::ifstream ifs(official_path);
    ASSERT(ifs.is_open());
    ifs.close();
}

} // namespace
