#include "core/batch_solver.hpp"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <fstream>
#include <random>
#include <sstream>
#include <stdexcept>
#include <thread>

namespace thermobench {
namespace batch {

std::vector<TestCase> generate_cases(size_t count, unsigned int seed, int N) {
    if (count == 0) {
        throw std::invalid_argument("Case count must be greater than zero.");
    }
    if (N < 2) {
        throw std::invalid_argument("Mesh N must be >= 2.");
    }

    std::vector<TestCase> cases;
    cases.reserve(count);

    std::mt19937 rng(seed);
    // k in [5.0, 50.0] W/(m*K)
    std::uniform_real_distribution<double> dist_k(5.0, 50.0);
    // q in [1000.0, 20000.0] W/m^3
    std::uniform_real_distribution<double> dist_q(1000.0, 20000.0);
    // T0 in [50.0, 150.0] C
    std::uniform_real_distribution<double> dist_t0(50.0, 150.0);
    // TL in [10.0, 40.0] C
    std::uniform_real_distribution<double> dist_tl(10.0, 40.0);

    for (size_t i = 0; i < count; ++i) {
        TestCase tc;
        tc.id = static_cast<int>(i);
        tc.N = N;
        tc.L = 1.0;
        tc.A = 1e-4;
        tc.k = dist_k(rng);
        tc.q = dist_q(rng);
        tc.T0 = dist_t0(rng);
        tc.TL = dist_tl(rng);
        cases.push_back(tc);
    }

    return cases;
}

BatchResult solve_serial(const std::vector<TestCase>& cases) {
    if (cases.empty()) {
        throw std::invalid_argument("Test cases vector is empty.");
    }

    auto start_time = std::chrono::high_resolution_clock::now();

    BatchResult result;
    result.results.resize(cases.size());
    result.threads_used = 1;

    for (size_t i = 0; i < cases.size(); ++i) {
        const auto& tc = cases[i];
        result.results[i].id = tc.id;
        result.results[i].sim = thermo::run_for_N(tc.L, tc.A, tc.k, tc.q, tc.T0, tc.TL, tc.N);
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    result.elapsed_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();

    return result;
}

BatchResult solve_parallel(const std::vector<TestCase>& cases, unsigned int num_threads) {
    if (cases.empty()) {
        throw std::invalid_argument("Test cases vector is empty.");
    }
    if (num_threads == 0) {
        throw std::invalid_argument("Thread count must be greater than zero.");
    }

    unsigned int hw = std::thread::hardware_concurrency();
    if (hw > 0 && num_threads > hw * 2) {
        num_threads = hw * 2;
    }

    auto start_time = std::chrono::high_resolution_clock::now();

    BatchResult result;
    result.results.resize(cases.size());
    result.threads_used = num_threads;

    const size_t total = cases.size();
    std::atomic<size_t> next_index{0};

    auto worker = [&]() {
        while (true) {
            size_t idx = next_index.fetch_add(1, std::memory_order_relaxed);
            if (idx >= total) {
                break;
            }
            const auto& tc = cases[idx];
            result.results[idx].id = tc.id;
            result.results[idx].sim = thermo::run_for_N(tc.L, tc.A, tc.k, tc.q, tc.T0, tc.TL, tc.N);
        }
    };

    std::vector<std::thread> threads;
    threads.reserve(num_threads);
    for (unsigned int t = 0; t < num_threads; ++t) {
        threads.emplace_back(worker);
    }

    for (auto& th : threads) {
        if (th.joinable()) {
            th.join();
        }
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    result.elapsed_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();

    return result;
}

double compare_results(const BatchResult& r1, const BatchResult& r2) {
    if (r1.results.size() != r2.results.size()) {
        throw std::invalid_argument("BatchResult sizes do not match.");
    }

    double max_diff = 0.0;
    for (size_t i = 0; i < r1.results.size(); ++i) {
        const auto& s1 = r1.results[i].sim;
        const auto& s2 = r2.results[i].sim;
        if (s1.T_fe.size() != s2.T_fe.size()) {
            throw std::invalid_argument("Node count mismatch in case result.");
        }
        for (size_t j = 0; j < s1.T_fe.size(); ++j) {
            double diff = std::abs(s1.T_fe[j] - s2.T_fe[j]);
            if (diff > max_diff) {
                max_diff = diff;
            }
        }
    }
    return max_diff;
}

BenchmarkSummary run_benchmark(const BenchmarkConfig& config) {
    if (config.case_count == 0) {
        throw std::invalid_argument("Benchmark case count must be > 0.");
    }
    if (config.thread_counts.empty()) {
        throw std::invalid_argument("Benchmark must have at least one thread count configured.");
    }

    auto cases = generate_cases(config.case_count, config.seed, config.mesh_N);

    // Warm-up run with serial solver
    for (int w = 0; w < config.warmup_runs; ++w) {
        auto warm = solve_serial(cases);
        (void)warm;
    }

    // Solve serial as baseline
    auto serial_res = solve_serial(cases);

    BenchmarkSummary summary;
    summary.case_count = config.case_count;
    summary.mesh_N = config.mesh_N;
    summary.max_discrepancy_C = 0.0;
    summary.parity_passed = true;

    double baseline_median_ms = 1.0;

    for (size_t ti = 0; ti < config.thread_counts.size(); ++ti) {
        unsigned int threads = config.thread_counts[ti];
        BenchmarkRun run;
        run.threads = threads;

        for (int r = 0; r < config.measured_runs; ++r) {
            BatchResult bres = (threads == 1) ? solve_serial(cases) : solve_parallel(cases, threads);
            run.run_times_ms.push_back(bres.elapsed_ms);

            // Verify parity against serial baseline
            double diff = compare_results(serial_res, bres);
            if (diff > summary.max_discrepancy_C) {
                summary.max_discrepancy_C = diff;
            }
            if (diff > 1e-8) {
                summary.parity_passed = false;
            }
        }

        // Calculate median
        auto sorted_times = run.run_times_ms;
        std::sort(sorted_times.begin(), sorted_times.end());
        size_t mid = sorted_times.size() / 2;
        if (sorted_times.size() % 2 == 0) {
            run.median_time_ms = 0.5 * (sorted_times[mid - 1] + sorted_times[mid]);
        } else {
            run.median_time_ms = sorted_times[mid];
        }

        if (threads == 1 || ti == 0) {
            baseline_median_ms = run.median_time_ms;
            run.speedup = 1.0;
            run.efficiency = 1.0;
        } else {
            run.speedup = baseline_median_ms / (run.median_time_ms > 1e-6 ? run.median_time_ms : 1e-6);
            run.efficiency = run.speedup / static_cast<double>(threads);
        }

        summary.runs.push_back(run);
    }

    return summary;
}

std::string to_csv(const BenchmarkSummary& summary) {
    std::ostringstream oss;
    oss << "threads,cases,mesh_N,run1_ms,run2_ms,run3_ms,run4_ms,run5_ms,median_ms,speedup,efficiency\n";
    for (const auto& r : summary.runs) {
        oss << r.threads << "," << summary.case_count << "," << summary.mesh_N;
        for (size_t i = 0; i < 5; ++i) {
            if (i < r.run_times_ms.size()) {
                oss << "," << r.run_times_ms[i];
            } else {
                oss << ",0.0";
            }
        }
        oss << "," << r.median_time_ms << "," << r.speedup << "," << r.efficiency << "\n";
    }
    return oss.str();
}

void export_benchmark_csv(const BenchmarkSummary& summary, const std::string& filepath) {
    std::string csv_data = to_csv(summary);
    std::ofstream ofs(filepath);
    if (!ofs.is_open()) {
        throw std::runtime_error("Failed to open file for writing: " + filepath);
    }
    ofs << csv_data;
    ofs.close();
}

} // namespace batch
} // namespace thermobench
