// src/core/backprop.cpp
// Implementation of pure C++17 Backpropagation network (no autodiff)

#include "backprop.hpp"
#include <cmath>
#include <stdexcept>
#include <sstream>
#include <fstream>
#include <algorithm>

namespace thermo {

ForwardResult forward(double x, double y, const NetworkParams& p) {
    if (!std::isfinite(x) || !std::isfinite(y) ||
        !std::isfinite(p.w) || !std::isfinite(p.b) ||
        !std::isfinite(p.v) || !std::isfinite(p.c)) {
        throw std::invalid_argument("forward: All inputs and parameters must be finite numbers.");
    }

    ForwardResult res;
    res.z = p.w * x + p.b;
    res.h = std::tanh(res.z);
    res.y_hat = p.v * res.h + p.c;
    const double diff = res.y_hat - y;
    res.loss = 0.5 * diff * diff;

    return res;
}

Gradients gradients(double x, double y, const NetworkParams& p) {
    ForwardResult fwd = forward(x, y, p);

    const double dJ_dyhat = fwd.y_hat - y;

    Gradients g;
    // dJ/dc = dJ/dy_hat * dy_hat/dc = (y_hat - y) * 1
    g.dc = dJ_dyhat;

    // dJ/dv = dJ/dy_hat * dy_hat/dv = (y_hat - y) * h
    g.dv = dJ_dyhat * fwd.h;

    // dJ/dh = dJ/dy_hat * dy_hat/dh = (y_hat - y) * v
    const double dJ_dh = dJ_dyhat * p.v;

    // dh/dz = 1 - tanh(z)^2 = 1 - h^2
    const double dh_dz = 1.0 - fwd.h * fwd.h;

    // delta_z = dJ/dz = dJ/dh * dh/dz
    const double delta_z = dJ_dh * dh_dz;

    // dJ/dw = delta_z * dz/dw = delta_z * x
    g.dw = delta_z * x;

    // dJ/db = delta_z * dz/db = delta_z * 1
    g.db = delta_z;

    return g;
}

double finite_diff_check(double x, double y, const NetworkParams& p, double eps) {
    if (!std::isfinite(eps) || eps <= 0.0) {
        throw std::invalid_argument("finite_diff_check: Perturbation step eps must be positive and finite.");
    }

    Gradients analytical = gradients(x, y, p);

    auto eval_loss = [&](const NetworkParams& params) -> double {
        return forward(x, y, params).loss;
    };

    // w
    NetworkParams pw_plus = p, pw_minus = p;
    pw_plus.w += eps;
    pw_minus.w -= eps;
    const double num_dw = (eval_loss(pw_plus) - eval_loss(pw_minus)) / (2.0 * eps);

    // b
    NetworkParams pb_plus = p, pb_minus = p;
    pb_plus.b += eps;
    pb_minus.b -= eps;
    const double num_db = (eval_loss(pb_plus) - eval_loss(pb_minus)) / (2.0 * eps);

    // v
    NetworkParams pv_plus = p, pv_minus = p;
    pv_plus.v += eps;
    pv_minus.v -= eps;
    const double num_dv = (eval_loss(pv_plus) - eval_loss(pv_minus)) / (2.0 * eps);

    // c
    NetworkParams pc_plus = p, pc_minus = p;
    pc_plus.c += eps;
    pc_minus.c -= eps;
    const double num_dc = (eval_loss(pc_plus) - eval_loss(pc_minus)) / (2.0 * eps);

    const double diff_w = std::abs(analytical.dw - num_dw);
    const double diff_b = std::abs(analytical.db - num_db);
    const double diff_v = std::abs(analytical.dv - num_dv);
    const double diff_c = std::abs(analytical.dc - num_dc);

    return std::max({diff_w, diff_b, diff_v, diff_c});
}

NetworkParams update_step(const NetworkParams& p, const Gradients& g, double lr) {
    if (!std::isfinite(lr) || lr <= 0.0) {
        throw std::invalid_argument("update_step: Learning rate must be positive and finite.");
    }
    if (!std::isfinite(g.dw) || !std::isfinite(g.db) || !std::isfinite(g.dv) || !std::isfinite(g.dc)) {
        throw std::invalid_argument("update_step: Gradients must be finite numbers.");
    }

    NetworkParams p_new;
    p_new.w = p.w - lr * g.dw;
    p_new.b = p.b - lr * g.db;
    p_new.v = p.v - lr * g.dv;
    p_new.c = p.c - lr * g.dc;

    return p_new;
}

std::vector<StepRecord> train_steps(double x, double y, NetworkParams p, double lr, int num_steps) {
    if (num_steps <= 0) {
        throw std::invalid_argument("train_steps: Number of steps must be positive (> 0).");
    }
    if (!std::isfinite(lr) || lr <= 0.0) {
        throw std::invalid_argument("train_steps: Learning rate must be positive and finite.");
    }

    std::vector<StepRecord> history;
    history.reserve(num_steps + 1);

    // Initial step 0
    ForwardResult fwd = forward(x, y, p);
    Gradients g = gradients(x, y, p);
    history.push_back({0, fwd.loss, fwd.y_hat, p, g});

    for (int step = 1; step <= num_steps; ++step) {
        p = update_step(p, g, lr);
        fwd = forward(x, y, p);
        g = gradients(x, y, p);
        history.push_back({step, fwd.loss, fwd.y_hat, p, g});
    }

    return history;
}

std::string loss_to_csv(const std::vector<StepRecord>& history) {
    if (history.empty()) {
        throw std::invalid_argument("loss_to_csv: Training history is empty.");
    }

    std::ostringstream oss;
    oss << "step,loss,y_hat,w,b,v,c,dw,db,dv,dc\n";
    oss.precision(8);
    oss << std::fixed;

    for (const auto& r : history) {
        oss << r.step << ","
            << r.loss << ","
            << r.y_hat << ","
            << r.params.w << ","
            << r.params.b << ","
            << r.params.v << ","
            << r.params.c << ","
            << r.grads.dw << ","
            << r.grads.db << ","
            << r.grads.dv << ","
            << r.grads.dc << "\n";
    }

    return oss.str();
}

void export_loss_csv(const std::vector<StepRecord>& history, const std::string& filepath) {
    if (filepath.empty()) {
        throw std::invalid_argument("export_loss_csv: File path must not be empty.");
    }
    std::string csv_content = loss_to_csv(history);
    std::ofstream ofs(filepath);
    if (!ofs.is_open()) {
        throw std::runtime_error("export_loss_csv: Failed to open file for writing: " + filepath);
    }
    ofs << csv_content;
}

} // namespace thermo
