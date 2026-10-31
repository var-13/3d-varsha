// src/core/backprop.hpp
// Phase 2: Single-hidden-neuron backpropagation network
// Architecture: x -> [ h = tanh(w*x + b) ] -> [ y_hat = v*h + c ]
// Loss: J = 0.5 * (y_hat - y)^2

#pragma once
#include <string>
#include <vector>

namespace thermo {

struct NetworkParams {
    double w = 0.5;
    double b = 0.0;
    double v = 1.0;
    double c = 0.0;
};

struct ForwardResult {
    double z = 0.0;     // pre-activation: w*x + b
    double h = 0.0;     // hidden activation: tanh(z)
    double y_hat = 0.0; // predicted output: v*h + c
    double loss = 0.0;  // squared error: 0.5 * (y_hat - y)^2
};

struct Gradients {
    double dw = 0.0;
    double db = 0.0;
    double dv = 0.0;
    double dc = 0.0;
};

struct StepRecord {
    int step = 0;
    double loss = 0.0;
    double y_hat = 0.0;
    NetworkParams params;
    Gradients grads;
};

// 1. Forward pass
// Computes z, h, y_hat, and loss J for scalar input x and target y.
// Throws std::invalid_argument if any parameter or input is non-finite.
ForwardResult forward(double x, double y, const NetworkParams& p);

// 2. Gradients (Analytical chain rule)
// Computes exact partial derivatives dJ/dw, dJ/db, dJ/dv, dJ/dc without autodiff.
Gradients gradients(double x, double y, const NetworkParams& p);

// 3. Central finite-difference check
// Checks analytical gradients against central difference with step eps (default 1e-6).
// Returns max absolute difference across all 4 parameters.
double finite_diff_check(double x, double y, const NetworkParams& p, double eps = 1e-6);

// 4. Update step (simultaneous parameter update)
// Computes updated parameters: theta_new = theta_old - lr * grad_old
NetworkParams update_step(const NetworkParams& p, const Gradients& g, double lr);

// 5. Train for N steps
// Executes N steps of gradient descent, recording loss and parameters at each step.
std::vector<StepRecord> train_steps(double x, double y, NetworkParams p, double lr, int num_steps);

// 6. CSV serialization & export
std::string loss_to_csv(const std::vector<StepRecord>& history);
void export_loss_csv(const std::vector<StepRecord>& history, const std::string& filepath);

} // namespace thermo
