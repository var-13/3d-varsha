// src/server/main.cpp
// Thin HTTP server for ThermoBench.
// ALL maths is in src/core – this file does routing and JSON only.
//
// Phase 0: serves ui/index.html + static assets; one /api/version endpoint.

// Pull in the full httplib implementation exactly once here.
// CPPHTTPLIB_OPENSSL_SUPPORT=0 is set via CMake target_compile_definitions.
#include "httplib.h"


#include <iostream>
#include <fstream>
#include <string>
#include <filesystem>
#include <thread>
#include <algorithm>
#include "core/fem1d.hpp"
#include "core/batch_solver.hpp"
#include "core/geometry.hpp"
#include "core/embedded_mock.hpp"

namespace fs = std::filesystem;

// ── Helpers ─────────────────────────────────────────────────────────────────

// Read a whole file into a string. Returns empty string on failure.
static std::string read_file(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    if (!f) return {};
    return {std::istreambuf_iterator<char>(f),
            std::istreambuf_iterator<char>()};
}

// Guess MIME type from file extension.
static std::string mime(const std::string& ext) {
    if (ext == ".html") return "text/html; charset=utf-8";
    if (ext == ".css")  return "text/css";
    if (ext == ".js")   return "application/javascript";
    if (ext == ".json") return "application/json";
    if (ext == ".svg")  return "image/svg+xml";
    if (ext == ".png")  return "image/png";
    if (ext == ".ico")  return "image/x-icon";
    return "application/octet-stream";
}

// ── Main ─────────────────────────────────────────────────────────────────────
int main(int argc, char* argv[]) {
    // The server expects to be run from the project root so that ui/ is reachable.
    // argv[1] can override the UI directory for packaging.
    fs::path ui_dir = (argc > 1) ? argv[1] : "ui";
    int port = (argc > 2) ? std::stoi(argv[2]) : 8080;

    if (!fs::exists(ui_dir)) {
        std::cerr << "[ThermoBench] ERROR: UI directory not found: "
                  << fs::absolute(ui_dir) << "\n";
        return 1;
    }

    httplib::Server svr;

    // ── /api/version ─────────────────────────────────────────────────────
    svr.Get("/api/version", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(
            "{\"version\":\"" + thermo::version() + "\"}",
            "application/json"
        );
    });

    // ── /health  (used by launcher to detect readiness) ──────────────────
    svr.Get("/health", [](const httplib::Request&, httplib::Response& res) {
        res.set_content("{\"status\":\"ok\"}", "application/json");
    });

    // ── /api/thermal/run ──────────────────────────────────────────────────
    // Query params: L, A, k, q, T0, TL, N
    svr.Get("/api/thermal/run", [](const httplib::Request& req, httplib::Response& res) {
        try {
            double L  = req.has_param("L")  ? std::stod(req.get_param_value("L"))  : 1.0;
            double A  = req.has_param("A")  ? std::stod(req.get_param_value("A"))  : 1e-4;
            double k  = req.has_param("k")  ? std::stod(req.get_param_value("k"))  : 10.0;
            double q  = req.has_param("q")  ? std::stod(req.get_param_value("q"))  : 10000.0;
            double T0 = req.has_param("T0") ? std::stod(req.get_param_value("T0")) : 100.0;
            double TL = req.has_param("TL") ? std::stod(req.get_param_value("TL")) : 20.0;
            int    N  = req.has_param("N")  ? std::stoi(req.get_param_value("N"))  : 10;

            thermo::SimulationResult sim = thermo::run_for_N(L, A, k, q, T0, TL, N);
            res.set_content(thermo::to_json(sim), "application/json");
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content("{\"success\":false,\"error\":\"" + std::string(e.what()) + "\"}", "application/json");
        }
    });

    // ── /api/thermal/convergence ─────────────────────────────────────────
    // Runs meshes N = 10, 20, 40, 80
    svr.Get("/api/thermal/convergence", [](const httplib::Request& req, httplib::Response& res) {
        try {
            double L  = req.has_param("L")  ? std::stod(req.get_param_value("L"))  : 1.0;
            double A  = req.has_param("A")  ? std::stod(req.get_param_value("A"))  : 1e-4;
            double k  = req.has_param("k")  ? std::stod(req.get_param_value("k"))  : 10.0;
            double q  = req.has_param("q")  ? std::stod(req.get_param_value("q"))  : 10000.0;
            double T0 = req.has_param("T0") ? std::stod(req.get_param_value("T0")) : 100.0;
            double TL = req.has_param("TL") ? std::stod(req.get_param_value("TL")) : 20.0;

            std::vector<thermo::SimulationResult> results;
            for (int N : {10, 20, 40, 80}) {
                results.push_back(thermo::run_for_N(L, A, k, q, T0, TL, N));
            }
            res.set_content(thermo::convergence_to_json(results), "application/json");
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content("{\"success\":false,\"error\":\"" + std::string(e.what()) + "\"}", "application/json");
        }
    });

    // ── /api/thermal/export ───────────────────────────────────────────────
    svr.Get("/api/thermal/export", [](const httplib::Request& req, httplib::Response& res) {
        try {
            double L  = req.has_param("L")  ? std::stod(req.get_param_value("L"))  : 1.0;
            double A  = req.has_param("A")  ? std::stod(req.get_param_value("A"))  : 1e-4;
            double k  = req.has_param("k")  ? std::stod(req.get_param_value("k"))  : 10.0;
            double q  = req.has_param("q")  ? std::stod(req.get_param_value("q"))  : 10000.0;
            double T0 = req.has_param("T0") ? std::stod(req.get_param_value("T0")) : 100.0;
            double TL = req.has_param("TL") ? std::stod(req.get_param_value("TL")) : 20.0;
            int    N  = req.has_param("N")  ? std::stoi(req.get_param_value("N"))  : 10;

            thermo::SimulationResult sim = thermo::run_for_N(L, A, k, q, T0, TL, N);
            res.set_header("Content-Disposition", "attachment; filename=\"thermal_N" + std::to_string(N) + ".csv\"");
            res.set_content(thermo::to_csv(sim), "text/csv");
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(std::string("Error: ") + e.what(), "text/plain");
        }
    });

    // ── /api/thermal/export_convergence ──────────────────────────────────
    svr.Get("/api/thermal/export_convergence", [](const httplib::Request& req, httplib::Response& res) {
        try {
            double L  = req.has_param("L")  ? std::stod(req.get_param_value("L"))  : 1.0;
            double A  = req.has_param("A")  ? std::stod(req.get_param_value("A"))  : 1e-4;
            double k  = req.has_param("k")  ? std::stod(req.get_param_value("k"))  : 10.0;
            double q  = req.has_param("q")  ? std::stod(req.get_param_value("q"))  : 10000.0;
            double T0 = req.has_param("T0") ? std::stod(req.get_param_value("T0")) : 100.0;
            double TL = req.has_param("TL") ? std::stod(req.get_param_value("TL")) : 20.0;

            std::vector<thermo::SimulationResult> results;
            for (int N : {10, 20, 40, 80}) {
                results.push_back(thermo::run_for_N(L, A, k, q, T0, TL, N));
            }
            res.set_header("Content-Disposition", "attachment; filename=\"thermal_convergence.csv\"");
            res.set_content(thermo::convergence_to_csv(results), "text/csv");
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(std::string("Error: ") + e.what(), "text/plain");
        }
    });

    // ── /api/benchmark/run ───────────────────────────────────────────────
    svr.Get("/api/benchmark/run", [](const httplib::Request& req, httplib::Response& res) {
        try {
            thermobench::batch::BenchmarkConfig cfg;
            cfg.case_count = req.has_param("cases") ? std::stoul(req.get_param_value("cases")) : 200;
            cfg.mesh_N = req.has_param("mesh") ? std::stoi(req.get_param_value("mesh")) : 40;
            cfg.seed = req.has_param("seed") ? static_cast<unsigned int>(std::stoul(req.get_param_value("seed"))) : 42;
            cfg.warmup_runs = 1;
            cfg.measured_runs = 5;

            unsigned int hw = std::thread::hardware_concurrency();
            if (hw == 0) hw = 4;
            std::vector<unsigned int> threads = {1};
            if (hw >= 2) threads.push_back(2);
            if (hw >= 4) threads.push_back(4);
            if (hw >= 8 && std::find(threads.begin(), threads.end(), 8) == threads.end()) threads.push_back(8);
            if (std::find(threads.begin(), threads.end(), hw) == threads.end()) threads.push_back(hw);
            std::sort(threads.begin(), threads.end());
            threads.erase(std::unique(threads.begin(), threads.end()), threads.end());

            cfg.thread_counts = threads;

            auto summary = thermobench::batch::run_benchmark(cfg);

            std::ostringstream json;
            json << "{\"case_count\":" << summary.case_count
                 << ",\"mesh_N\":" << summary.mesh_N
                 << ",\"parity_passed\":" << (summary.parity_passed ? "true" : "false")
                 << ",\"max_discrepancy_C\":" << summary.max_discrepancy_C
                 << ",\"runs\":[";

            for (size_t i = 0; i < summary.runs.size(); ++i) {
                const auto& r = summary.runs[i];
                if (i > 0) json << ",";
                json << "{\"threads\":" << r.threads
                     << ",\"median_ms\":" << r.median_time_ms
                     << ",\"speedup\":" << r.speedup
                     << ",\"efficiency\":" << r.efficiency
                     << ",\"run_times_ms\":[";
                for (size_t j = 0; j < r.run_times_ms.size(); ++j) {
                    if (j > 0) json << ",";
                    json << r.run_times_ms[j];
                }
                json << "]}";
            }
            json << "]}";

            res.set_content(json.str(), "application/json");
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content("{\"error\":\"" + std::string(e.what()) + "\"}", "application/json");
        }
    });

    // ── /api/benchmark/export ────────────────────────────────────────────
    svr.Get("/api/benchmark/export", [](const httplib::Request& req, httplib::Response& res) {
        try {
            thermobench::batch::BenchmarkConfig cfg;
            cfg.case_count = req.has_param("cases") ? std::stoul(req.get_param_value("cases")) : 200;
            cfg.mesh_N = req.has_param("mesh") ? std::stoi(req.get_param_value("mesh")) : 40;
            cfg.seed = req.has_param("seed") ? static_cast<unsigned int>(std::stoul(req.get_param_value("seed"))) : 42;
            cfg.warmup_runs = 1;
            cfg.measured_runs = 5;

            unsigned int hw = std::thread::hardware_concurrency();
            if (hw == 0) hw = 4;
            std::vector<unsigned int> threads = {1};
            if (hw >= 2) threads.push_back(2);
            if (hw >= 4) threads.push_back(4);
            cfg.thread_counts = threads;

            auto summary = thermobench::batch::run_benchmark(cfg);
            res.set_header("Content-Disposition", "attachment; filename=\"benchmark_multicore.csv\"");
            res.set_content(thermobench::batch::to_csv(summary), "text/csv");
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(std::string("Error: ") + e.what(), "text/plain");
        }
    });

    // ── /api/geometry/stl ────────────────────────────────────────────────
    svr.Get("/api/geometry/stl", [](const httplib::Request&, httplib::Response& res) {
        try {
            auto mesh = thermobench::geometry::parse_stl_file("data/sample.stl");
            res.set_content(thermobench::geometry::stl_to_json(mesh), "application/json");
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content("{\"error\":\"" + std::string(e.what()) + "\"}", "application/json");
        }
    });

    // ── /api/geometry/pointcloud ─────────────────────────────────────────
    svr.Get("/api/geometry/pointcloud", [](const httplib::Request&, httplib::Response& res) {
        try {
            auto pc = thermobench::geometry::parse_ansys_csv_file("data/sample_ansys.csv");
            res.set_content(thermobench::geometry::pointcloud_to_json(pc), "application/json");
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content("{\"error\":\"" + std::string(e.what()) + "\"}", "application/json");
        }
    });

    // ── /api/geometry/stl_raw ────────────────────────────────────────────
    svr.Get("/api/geometry/stl_raw", [](const httplib::Request&, httplib::Response& res) {
        std::string raw = read_file("data/sample.stl");
        if (raw.empty()) {
            res.status = 404;
            res.set_content("data/sample.stl not found", "text/plain");
        } else {
            res.set_header("Content-Disposition", "attachment; filename=\"sample.stl\"");
            res.set_content(raw, "model/stl");
        }
    });

    // ── /api/geometry/csv_raw ────────────────────────────────────────────
    svr.Get("/api/geometry/csv_raw", [](const httplib::Request&, httplib::Response& res) {
        std::string raw = read_file("data/sample_ansys.csv");
        if (raw.empty()) {
            res.status = 404;
            res.set_content("data/sample_ansys.csv not found", "text/plain");
        } else {
            res.set_header("Content-Disposition", "attachment; filename=\"sample_ansys.csv\"");
            res.set_content(raw, "text/csv");
        }
    });

    // ── /api/hardware/devices ─────────────────────────────────────────────
    svr.Get("/api/hardware/devices", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(
            thermobench::embedded::EmbeddedManager::instance().devices_to_json(),
            "application/json"
        );
    });

    // ── /api/hardware/telemetry_all ──────────────────────────────────────
    svr.Get("/api/hardware/telemetry_all", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(
            thermobench::embedded::EmbeddedManager::instance().all_telemetry_to_json(),
            "application/json"
        );
    });

    // ── /api/hardware/telemetry/:id ──────────────────────────────────────
    svr.Get("/api/hardware/telemetry/:id", [](const httplib::Request& req, httplib::Response& res) {
        const std::string id = req.path_params.at("id");
        try {
            res.set_content(
                thermobench::embedded::EmbeddedManager::instance().telemetry_to_json(id),
                "application/json"
            );
        } catch (const std::exception& e) {
            res.status = 404;
            res.set_content("{\"error\":\"" + std::string(e.what()) + "\"}", "application/json");
        }
    });

    // ── POST /api/hardware/timeout/:id ───────────────────────────────────
    svr.Post("/api/hardware/timeout/:id", [](const httplib::Request& req, httplib::Response& res) {
        const std::string id = req.path_params.at("id");
        bool ok = thermobench::embedded::EmbeddedManager::instance().trigger_timeout(id, 500);
        res.set_content(ok
            ? "{\"success\":true,\"msg\":\"Timeout injected\"}"
            : "{\"success\":false,\"msg\":\"Device not found\"}",
            "application/json"
        );
    });

    // ── POST /api/hardware/recover/:id ───────────────────────────────────
    svr.Post("/api/hardware/recover/:id", [](const httplib::Request& req, httplib::Response& res) {
        const std::string id = req.path_params.at("id");
        bool ok = thermobench::embedded::EmbeddedManager::instance().recover_device(id);
        res.set_content(ok
            ? "{\"success\":true,\"msg\":\"Device recovered\"}"
            : "{\"success\":false,\"msg\":\"Device not found\"}",
            "application/json"
        );
    });

    // ── POST /api/hardware/reset ──────────────────────────────────────────
    svr.Post("/api/hardware/reset", [](const httplib::Request&, httplib::Response& res) {
        thermobench::embedded::EmbeddedManager::instance().reset();
        res.set_content("{\"success\":true,\"msg\":\"All devices reset\"}", "application/json");
    });

    // ── Static file serving ───────────────────────────────────────────────

    svr.Get("/.*", [&ui_dir](const httplib::Request& req, httplib::Response& res) {
        // Strip leading '/'
        std::string rel = req.path.substr(1);
        if (rel.empty()) rel = "index.html";

        fs::path file = ui_dir / rel;
        std::string body = read_file(file);
        if (body.empty()) {
            res.status = 404;
            res.set_content("404 Not Found: " + rel, "text/plain");
            return;
        }
        std::string ext = file.extension().string();
        res.set_content(body, mime(ext));
    });

    std::cout << "[ThermoBench] Serving " << fs::absolute(ui_dir)
              << " on http://localhost:" << port << "\n";
    std::cout << "[ThermoBench] Press Ctrl+C to stop.\n";

    if (!svr.listen("localhost", port)) {
        std::cerr << "[ThermoBench] ERROR: Could not bind to port " << port
                  << ". Is something else running there?\n";
        return 1;
    }
    return 0;
}
