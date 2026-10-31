// src/core/fem1d.hpp
// Phase 0 stub – just enough to compile and link.
// Real implementation added in Phase 1.
#pragma once
#include <string>
#include <vector>

namespace thermo {

// Returns a human-readable version string (used by the hello-world server).
std::string version();

// ── Phase 1: 1D Steady-State Thermal Finite Element Analysis ─────────────────
// Governing equation: -d/dx(k*A * dT/dx) = q*A on x in [0, L]
// Using 2-node linear elements with shape functions N1(x) = 1 - s, N2(x) = s (s = x/h)

struct ElementMatrices {
    double Ke[2][2]; // 2x2 local conductance matrix [W/K]
    double fe[2];    // 2x1 local thermal load vector [W]
};

// Assembles the 2x2 stiffness (conductance) matrix and 2x1 load vector for one element.
// Ke = (k*A/h) * [[ 1, -1], [-1,  1]]
// fe = (q*A*h / 2) * [1, 1]
// Throws std::invalid_argument if h <= 0, k <= 0, A <= 0, or any value is non-finite (NaN/Inf).
ElementMatrices assemble_element(double k, double A, double h, double q);

struct GlobalSystem {
    int num_nodes = 0;
    std::vector<std::vector<double>> K; // (num_nodes x num_nodes) global conductance matrix
    std::vector<double> F;              // (num_nodes) global thermal load vector
};

// Assembles global stiffness matrix K and load vector F for N uniform elements on [0, L].
// Node 0 is at x = 0; Node N is at x = L.
// Element e connects node e and node e+1, with h = L / N.
// Throws std::invalid_argument if N <= 0, L <= 0, A <= 0, k <= 0, or any input is non-finite.
GlobalSystem assemble_global(double L, double A, double k, double q, int N);

// Imposes Dirichlet boundary conditions T(0) = T0 and T(L) = TL by row and column elimination:
// 1. Column elimination: Known boundary values are moved to the right-hand side:
//    F_1 -= K_{1,0} * T0;  F_{N-1} -= K_{N-1, N} * TL
// 2. Off-diagonal boundary entries are zeroed to preserve matrix symmetry:
//    K_{0,1} = K_{1,0} = 0;  K_{N, N-1} = K_{N-1, N} = 0
// 3. Diagonal boundary entries set to 1.0, and RHS set to prescribed values:
//    K_{0,0} = 1.0, F_0 = T0;  K_{N,N} = 1.0, F_N = TL
// Throws std::invalid_argument if sys.num_nodes < 2 or boundary temperatures are non-finite.
void apply_boundary_conditions(GlobalSystem& sys, double T0, double TL);

// Solves K * T = F using the Thomas tridiagonal algorithm (TDMA) in O(N) time and O(N) space.
// Chosen over dense solver (O(N^3) time, O(N^2) space) because:
// 1. The 1D FEM conductance matrix is strictly tridiagonal.
// 2. The matrix is diagonally dominant and positive-definite, guaranteeing unconditional stability without pivoting.
// Returns vector of nodal temperatures T of size sys.num_nodes.
// Throws std::invalid_argument if sys.num_nodes < 2, or std::runtime_error if singular matrix.
std::vector<double> solve(const GlobalSystem& sys);

// Evaluates the exact analytical temperature at coordinate x:
// T(x) = T0 + ((TL - T0) / L) * x + (q / (2*k)) * x * (L - x)
// Throws std::invalid_argument if L <= 0, k <= 0, x < 0, x > L, or any input is non-finite.
double exact_solution(double x, double L, double k, double q, double T0, double TL);

struct MidpointErrorResult {
    double max_error = 0.0;
    std::vector<double> x_mid;
    std::vector<double> T_fe_mid;
    std::vector<double> T_exact_mid;
    std::vector<double> errors;
};

// Computes the maximum absolute error at element midpoints using FE linear interpolation:
// T_fe(x_mid) = (T[e] + T[e+1]) / 2.
// Analytical expectation: max_error = (q / (2*k)) * (h / 2)^2 = 125 * h^2
// Throws std::invalid_argument if T.size() < 2 or parameters are non-finite/non-positive.
MidpointErrorResult compute_midpoint_errors(const std::vector<double>& T, double L, double k, double q, double T0, double TL);
double midpoint_error(const std::vector<double>& T, double L, double k, double q, double T0, double TL);

struct SimulationResult {
    int N = 0;
    double L = 0.0;
    double A = 0.0;
    double k = 0.0;
    double q = 0.0;
    double T0 = 0.0;
    double TL = 0.0;
    double h = 0.0;

    // Nodal values (size N + 1)
    std::vector<double> x_nodes;
    std::vector<double> T_fe;
    std::vector<double> T_exact;
    std::vector<double> nodal_errors;

    // Midpoint values (size N)
    std::vector<double> x_mid;
    std::vector<double> T_fe_mid;
    std::vector<double> T_exact_mid;
    std::vector<double> midpoint_errors;

    double max_nodal_error = 0.0;
    double max_midpoint_error = 0.0;
};

// Executes the complete pipeline for mesh size N:
// 1. assemble_global(L, A, k, q, N)
// 2. apply_boundary_conditions(sys, T0, TL)
// 3. solve(sys)
// 4. exact_solution(...) at nodes and midpoints
// 5. compute_midpoint_errors(...)
// Throws std::invalid_argument if N <= 0, L <= 0, A <= 0, k <= 0, or any input is non-finite.
SimulationResult run_for_N(double L, double A, double k, double q, double T0, double TL, int N);

// Serializes a simulation result to CSV string:
// Header: x_m,T_fe_C,T_exact_C,error_C
std::string to_csv(const SimulationResult& res);

// Writes simulation result to a CSV file.
// Throws std::invalid_argument if filepath is empty, std::runtime_error if file cannot be opened.
void export_csv(const SimulationResult& res, const std::string& filepath);

// Serializes convergence data across multiple meshes to CSV string:
// Header: N,h_m,max_nodal_error_C,max_midpoint_error_C,observed_order
std::string convergence_to_csv(const std::vector<SimulationResult>& results);

// Writes convergence data to a CSV file.
// Throws std::invalid_argument if filepath is empty, std::runtime_error if file cannot be opened.
void export_convergence_csv(const std::vector<SimulationResult>& results, const std::string& filepath);

// Serializes simulation result to JSON string.
std::string to_json(const SimulationResult& res);

// Serializes convergence results across meshes to JSON string.
std::string convergence_to_json(const std::vector<SimulationResult>& results);

} // namespace thermo
