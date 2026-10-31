#include "test_runner.hpp"
#include "core/backprop.hpp"
#include <cmath>
#include <limits>
#include <fstream>

// ── Function 1: forward ──────────────────────────────────────────────────────

TEST("Phase 2 - forward: verifies initial state against analytical values") {
    const double x = 1.0;
    const double y = 2.0;
    thermo::NetworkParams p;
    p.w = 0.5;
    p.b = 0.0;
    p.v = 1.0;
    p.c = 0.0;

    thermo::ForwardResult res = thermo::forward(x, y, p);

    // z = w*x + b = 0.5*1 + 0 = 0.5
    ASSERT_NEAR(res.z, 0.5, 1e-12);

    // h = tanh(0.5) ~ 0.46211715726
    const double expected_h = std::tanh(0.5);
    ASSERT_NEAR(res.h, expected_h, 1e-12);
    ASSERT_NEAR(res.h, 0.4621, 1e-3); // Brief check

    // y_hat = v*h + c = 1.0*h + 0 = h
    ASSERT_NEAR(res.y_hat, expected_h, 1e-12);
    ASSERT_NEAR(res.y_hat, 0.4621, 1e-3); // Brief check

    // J = 0.5 * (y_hat - y)^2 = 0.5 * (0.462117 - 2)^2 ~ 1.1825418
    const double diff = expected_h - 2.0;
    const double expected_J = 0.5 * diff * diff;
    ASSERT_NEAR(res.loss, expected_J, 1e-12);
    ASSERT_NEAR(res.loss, 1.1825, 1e-3); // Brief check
}

TEST("Phase 2 - forward: non-finite inputs throw invalid_argument") {
    const double nan_val = std::numeric_limits<double>::quiet_NaN();
    thermo::NetworkParams p;

    ASSERT_THROWS(thermo::forward(nan_val, 2.0, p));
    ASSERT_THROWS(thermo::forward(1.0, nan_val, p));

    thermo::NetworkParams bad_p = p;
    bad_p.w = nan_val;
    ASSERT_THROWS(thermo::forward(1.0, 2.0, bad_p));
}

// ── Function 2: gradients ────────────────────────────────────────────────────

TEST("Phase 2 - gradients: analytical chain rule matches brief values") {
    const double x = 1.0;
    const double y = 2.0;
    thermo::NetworkParams p;
    p.w = 0.5;
    p.b = 0.0;
    p.v = 1.0;
    p.c = 0.0;

    thermo::Gradients g = thermo::gradients(x, y, p);

    // Approximate checks from brief:
    // dc ~ -1.5379, dv ~ -0.7107, dw = db ~ -1.2095
    ASSERT_NEAR(g.dc, -1.5379, 1e-3);
    ASSERT_NEAR(g.dv, -0.7107, 1e-3);
    ASSERT_NEAR(g.dw, -1.2095, 1e-3);
    ASSERT_NEAR(g.db, -1.2095, 1e-3);

    // Precise analytical check:
    const double h = std::tanh(0.5);
    const double dyhat = h - 2.0;
    const double expected_dc = dyhat;
    const double expected_dv = dyhat * h;
    const double expected_dz = dyhat * 1.0 * (1.0 - h * h);
    const double expected_dw = expected_dz * 1.0;
    const double expected_db = expected_dz;

    ASSERT_NEAR(g.dc, expected_dc, 1e-12);
    ASSERT_NEAR(g.dv, expected_dv, 1e-12);
    ASSERT_NEAR(g.dw, expected_dw, 1e-12);
    ASSERT_NEAR(g.db, expected_db, 1e-12);

    // Since x = 1.0, dw must equal db exactly
    ASSERT_NEAR(g.dw, g.db, 1e-12);
}

TEST("Phase 2 - gradients: zero error produces zero gradients") {
    thermo::NetworkParams p;
    // Set target y equal to forward prediction
    thermo::ForwardResult fwd = thermo::forward(1.0, 0.0, p);
    thermo::Gradients g = thermo::gradients(1.0, fwd.y_hat, p);

    ASSERT_NEAR(g.dw, 0.0, 1e-12);
    ASSERT_NEAR(g.db, 0.0, 1e-12);
    ASSERT_NEAR(g.dv, 0.0, 1e-12);
    ASSERT_NEAR(g.dc, 0.0, 1e-12);
}

TEST("Phase 2 - gradients: non-finite inputs throw invalid_argument") {
    const double nan_val = std::numeric_limits<double>::quiet_NaN();
    thermo::NetworkParams p;
    ASSERT_THROWS(thermo::gradients(nan_val, 2.0, p));
}

// ── Function 3: finite_diff_check ────────────────────────────────────────────

TEST("Phase 2 - finite_diff_check: central difference error <= 1e-5 for all four parameters") {
    const double x = 1.0;
    const double y = 2.0;
    thermo::NetworkParams p;
    p.w = 0.5;
    p.b = 0.0;
    p.v = 1.0;
    p.c = 0.0;

    const double max_err = thermo::finite_diff_check(x, y, p, 1e-6);

    // Brief requirement: assert abs error <= 1e-5 for all four
    ASSERT(max_err <= 1e-5);
    // In practice, central difference is accurate to ~1e-10
    ASSERT(max_err < 1e-9);
}

TEST("Phase 2 - finite_diff_check: holds across arbitrary non-zero parameters") {
    thermo::NetworkParams p;
    p.w = 1.25;
    p.b = -0.4;
    p.v = 2.1;
    p.c = 0.75;

    const double max_err = thermo::finite_diff_check(0.85, 1.5, p, 1e-6);
    ASSERT(max_err <= 1e-5);
}

TEST("Phase 2 - finite_diff_check: non-positive eps throws invalid_argument") {
    thermo::NetworkParams p;
    ASSERT_THROWS(thermo::finite_diff_check(1.0, 2.0, p, 0.0));
    ASSERT_THROWS(thermo::finite_diff_check(1.0, 2.0, p, -1e-6));
}

// ── Function 4: update_step ──────────────────────────────────────────────────

TEST("Phase 2 - update_step: simultaneous update matches brief values exactly") {
    const double x = 1.0;
    const double y = 2.0;
    thermo::NetworkParams p0;
    p0.w = 0.5;
    p0.b = 0.0;
    p0.v = 1.0;
    p0.c = 0.0;

    const double lr = 0.1;
    thermo::Gradients g = thermo::gradients(x, y, p0);
    thermo::NetworkParams p1 = thermo::update_step(p0, g, lr);

    // Brief values: w ~ 0.6209, b ~ 0.1209, v ~ 1.0711, c ~ 0.1538
    ASSERT_NEAR(p1.w, 0.6209, 1e-3);
    ASSERT_NEAR(p1.b, 0.1209, 1e-3);
    ASSERT_NEAR(p1.v, 1.0711, 1e-3);
    ASSERT_NEAR(p1.c, 0.1538, 1e-3);

    // Exact formula check:
    ASSERT_NEAR(p1.w, p0.w - lr * g.dw, 1e-12);
    ASSERT_NEAR(p1.b, p0.b - lr * g.db, 1e-12);
    ASSERT_NEAR(p1.v, p0.v - lr * g.dv, 1e-12);
    ASSERT_NEAR(p1.c, p0.c - lr * g.dc, 1e-12);

    // Verify loss decreases after one step
    thermo::ForwardResult fwd0 = thermo::forward(x, y, p0);
    thermo::ForwardResult fwd1 = thermo::forward(x, y, p1);
    ASSERT(fwd1.loss < fwd0.loss);
}

TEST("Phase 2 - update_step: non-positive or non-finite lr throws invalid_argument") {
    thermo::NetworkParams p;
    thermo::Gradients g;
    ASSERT_THROWS(thermo::update_step(p, g, 0.0));
    ASSERT_THROWS(thermo::update_step(p, g, -0.1));
}

// ── Function 5: train_steps ──────────────────────────────────────────────────

TEST("Phase 2 - train_steps: 100 steps of gradient descent convergence") {
    const double x = 1.0;
    const double y = 2.0;
    thermo::NetworkParams p0;
    p0.w = 0.5;
    p0.b = 0.0;
    p0.v = 1.0;
    p0.c = 0.0;

    const double lr = 0.1;
    const int num_steps = 100;

    std::vector<thermo::StepRecord> history = thermo::train_steps(x, y, p0, lr, num_steps);

    ASSERT(history.size() == static_cast<size_t>(num_steps + 1));
    ASSERT(history[0].step == 0);
    ASSERT(history[100].step == 100);

    // Initial loss
    ASSERT_NEAR(history[0].loss, 1.1825, 1e-3);

    // Step 1 matches brief
    ASSERT_NEAR(history[1].params.w, 0.6209, 1e-3);
    ASSERT_NEAR(history[1].params.b, 0.1209, 1e-3);
    ASSERT_NEAR(history[1].params.v, 1.0711, 1e-3);
    ASSERT_NEAR(history[1].params.c, 0.1538, 1e-3);

    // Loss strictly decreases over training
    ASSERT(history[100].loss < history[50].loss);
    ASSERT(history[50].loss < history[0].loss);

    // Final loss is small (network converges toward target 2.0)
    ASSERT(history[100].loss < 0.05);
    ASSERT_NEAR(history[100].y_hat, 2.0, 0.35);
}

TEST("Phase 2 - train_steps: invalid steps or lr throws invalid_argument") {
    thermo::NetworkParams p;
    ASSERT_THROWS(thermo::train_steps(1.0, 2.0, p, 0.1, 0));
    ASSERT_THROWS(thermo::train_steps(1.0, 2.0, p, 0.1, -10));
    ASSERT_THROWS(thermo::train_steps(1.0, 2.0, p, -0.1, 100));
}

// ── Function 6: export_loss_csv ──────────────────────────────────────────────

TEST("Phase 2 - export_loss_csv: serializes and writes official results/backprop_loss.csv") {
    const double x = 1.0;
    const double y = 2.0;
    thermo::NetworkParams p0;
    p0.w = 0.5;
    p0.b = 0.0;
    p0.v = 1.0;
    p0.c = 0.0;

    std::vector<thermo::StepRecord> history = thermo::train_steps(x, y, p0, 0.1, 100);

    // 1. Serialization check
    std::string csv = thermo::loss_to_csv(history);
    ASSERT(!csv.empty());
    ASSERT(csv.find("step,loss,y_hat,w,b,v,c,dw,db,dv,dc\n") == 0);

    // 2. Export official file to results/backprop_loss.csv
    const std::string filepath = "results/backprop_loss.csv";
    thermo::export_loss_csv(history, filepath);

    // 3. Verify file exists and has 102 lines (header + 101 records)
    std::ifstream ifs(filepath);
    ASSERT(ifs.is_open());
    int line_count = 0;
    std::string line;
    while (std::getline(ifs, line)) {
        if (!line.empty()) ++line_count;
    }
    ASSERT(line_count == 102);
}

TEST("Phase 2 - export_loss_csv: invalid arguments throw invalid_argument") {
    std::vector<thermo::StepRecord> empty_hist;
    ASSERT_THROWS(thermo::loss_to_csv(empty_hist));
    ASSERT_THROWS(thermo::export_loss_csv(empty_hist, "results/test.csv"));

    thermo::NetworkParams p;
    std::vector<thermo::StepRecord> valid_hist = thermo::train_steps(1.0, 2.0, p, 0.1, 5);
    ASSERT_THROWS(thermo::export_loss_csv(valid_hist, ""));
}
