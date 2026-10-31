#include "fem1d.hpp"
#include <cmath>
#include <stdexcept>
#include <sstream>
#include <fstream>

namespace thermo {

std::string version() {
    return "ThermoBench v0.1.0 (Phase 1: Thermal FEA)";
}

ElementMatrices assemble_element(double k, double A, double h, double q) {
    if (!std::isfinite(k) || !std::isfinite(A) || !std::isfinite(h) || !std::isfinite(q)) {
        throw std::invalid_argument("assemble_element: All parameters (k, A, h, q) must be finite real numbers.");
    }
    if (k <= 0.0) {
        std::ostringstream oss;
        oss << "assemble_element: Thermal conductivity k must be positive (> 0), got " << k;
        throw std::invalid_argument(oss.str());
    }
    if (A <= 0.0) {
        std::ostringstream oss;
        oss << "assemble_element: Area A must be positive (> 0), got " << A;
        throw std::invalid_argument(oss.str());
    }
    if (h <= 0.0) {
        std::ostringstream oss;
        oss << "assemble_element: Element length h must be positive (> 0), got " << h;
        throw std::invalid_argument(oss.str());
    }

    ElementMatrices em;
    const double stiff = (k * A) / h;
    em.Ke[0][0] =  stiff;
    em.Ke[0][1] = -stiff;
    em.Ke[1][0] = -stiff;
    em.Ke[1][1] =  stiff;

    const double load = (q * A * h) / 2.0;
    em.fe[0] = load;
    em.fe[1] = load;

    return em;
}

GlobalSystem assemble_global(double L, double A, double k, double q, int N) {
    if (N <= 0) {
        std::ostringstream oss;
        oss << "assemble_global: Number of elements N must be positive (> 0), got " << N;
        throw std::invalid_argument(oss.str());
    }
    if (!std::isfinite(L) || !std::isfinite(A) || !std::isfinite(k) || !std::isfinite(q)) {
        throw std::invalid_argument("assemble_global: All physical parameters (L, A, k, q) must be finite real numbers.");
    }
    if (L <= 0.0) {
        std::ostringstream oss;
        oss << "assemble_global: Domain length L must be positive (> 0), got " << L;
        throw std::invalid_argument(oss.str());
    }
    if (A <= 0.0) {
        std::ostringstream oss;
        oss << "assemble_global: Area A must be positive (> 0), got " << A;
        throw std::invalid_argument(oss.str());
    }
    if (k <= 0.0) {
        std::ostringstream oss;
        oss << "assemble_global: Thermal conductivity k must be positive (> 0), got " << k;
        throw std::invalid_argument(oss.str());
    }

    const double h = L / static_cast<double>(N);
    ElementMatrices em = assemble_element(k, A, h, q);

    const int num_nodes = N + 1;
    GlobalSystem sys;
    sys.num_nodes = num_nodes;
    sys.K.assign(num_nodes, std::vector<double>(num_nodes, 0.0));
    sys.F.assign(num_nodes, 0.0);

    for (int e = 0; e < N; ++e) {
        const int n0 = e;
        const int n1 = e + 1;

        sys.K[n0][n0] += em.Ke[0][0];
        sys.K[n0][n1] += em.Ke[0][1];
        sys.K[n1][n0] += em.Ke[1][0];
        sys.K[n1][n1] += em.Ke[1][1];

        sys.F[n0] += em.fe[0];
        sys.F[n1] += em.fe[1];
    }

    return sys;
}

void apply_boundary_conditions(GlobalSystem& sys, double T0, double TL) {
    if (sys.num_nodes < 2) {
        throw std::invalid_argument("apply_boundary_conditions: System must have at least 2 nodes.");
    }
    if (!std::isfinite(T0) || !std::isfinite(TL)) {
        throw std::invalid_argument("apply_boundary_conditions: Boundary temperatures T0 and TL must be finite.");
    }
    const int N_node = sys.num_nodes - 1;

    // 1. Column elimination: subtract column contributions of node 0 and node N_node from interior RHS
    // For node 0: affects node 1
    sys.F[1] -= sys.K[1][0] * T0;
    // For node N_node: affects node N_node - 1
    sys.F[N_node - 1] -= sys.K[N_node - 1][N_node] * TL;

    // 2. Zero off-diagonals in rows and columns 0 and N_node to preserve symmetry
    sys.K[0][1] = 0.0;
    sys.K[1][0] = 0.0;
    sys.K[N_node][N_node - 1] = 0.0;
    sys.K[N_node - 1][N_node] = 0.0;

    // 3. Set diagonal entries to 1.0 and RHS to Dirichlet values
    sys.K[0][0] = 1.0;
    sys.F[0] = T0;

    sys.K[N_node][N_node] = 1.0;
    sys.F[N_node] = TL;
}

std::vector<double> solve(const GlobalSystem& sys) {
    if (sys.num_nodes < 2) {
        throw std::invalid_argument("solve: System must have at least 2 nodes.");
    }
    const int n = sys.num_nodes;
    std::vector<double> T(n, 0.0);

    // Boundary nodes are already decoupled by apply_boundary_conditions
    T[0] = sys.F[0];
    T[n - 1] = sys.F[n - 1];

    const int M = n - 2; // Number of interior unknowns (indices 1 to n-2)
    if (M == 0) {
        return T;
    }

    // Vectors for interior equations:
    // a[i]: subdiagonal (coefficient of x_{i-1}) for i = 1 .. M-1
    // b[i]: main diagonal (coefficient of x_i) for i = 0 .. M-1
    // c[i]: superdiagonal (coefficient of x_{i+1}) for i = 0 .. M-2
    // d[i]: right-hand side for i = 0 .. M-1
    std::vector<double> a(M, 0.0);
    std::vector<double> b(M, 0.0);
    std::vector<double> c(M, 0.0);
    std::vector<double> d(M, 0.0);

    for (int i = 0; i < M; ++i) {
        const int node = i + 1;
        b[i] = sys.K[node][node];
        d[i] = sys.F[node];
        if (i > 0) {
            a[i] = sys.K[node][node - 1];
        }
        if (i < M - 1) {
            c[i] = sys.K[node][node + 1];
        }
    }

    // Thomas algorithm forward sweep
    std::vector<double> c_prime(M, 0.0);
    std::vector<double> d_prime(M, 0.0);

    if (std::abs(b[0]) < 1e-15) {
        throw std::runtime_error("solve: Zero pivot encountered in Thomas forward elimination.");
    }
    c_prime[0] = c[0] / b[0];
    d_prime[0] = d[0] / b[0];

    for (int i = 1; i < M; ++i) {
        const double denom = b[i] - a[i] * c_prime[i - 1];
        if (std::abs(denom) < 1e-15) {
            throw std::runtime_error("solve: Zero pivot encountered in Thomas forward elimination.");
        }
        if (i < M - 1) {
            c_prime[i] = c[i] / denom;
        }
        d_prime[i] = (d[i] - a[i] * d_prime[i - 1]) / denom;
    }

    // Back substitution
    std::vector<double> x(M, 0.0);
    x[M - 1] = d_prime[M - 1];
    for (int i = M - 2; i >= 0; --i) {
        x[i] = d_prime[i] - c_prime[i] * x[i + 1];
    }

    // Map back into full solution vector
    for (int i = 0; i < M; ++i) {
        T[i + 1] = x[i];
    }

    return T;
}

double exact_solution(double x, double L, double k, double q, double T0, double TL) {
    if (!std::isfinite(x) || !std::isfinite(L) || !std::isfinite(k) ||
        !std::isfinite(q) || !std::isfinite(T0) || !std::isfinite(TL)) {
        throw std::invalid_argument("exact_solution: All arguments must be finite real numbers.");
    }
    if (L <= 0.0) {
        throw std::invalid_argument("exact_solution: Domain length L must be positive.");
    }
    if (k <= 0.0) {
        throw std::invalid_argument("exact_solution: Thermal conductivity k must be positive.");
    }
    if (x < -1e-12 || x > L + 1e-12) {
        std::ostringstream oss;
        oss << "exact_solution: Coordinate x=" << x << " is outside the domain [0, " << L << "].";
        throw std::invalid_argument(oss.str());
    }

    // Clamp tiny numerical overflow at boundary
    if (x < 0.0) x = 0.0;
    if (x > L) x = L;

    const double linear_part = T0 + ((TL - T0) / L) * x;
    const double source_part = (q / (2.0 * k)) * x * (L - x);
    return linear_part + source_part;
}

MidpointErrorResult compute_midpoint_errors(const std::vector<double>& T, double L, double k, double q, double T0, double TL) {
    if (T.size() < 2) {
        throw std::invalid_argument("compute_midpoint_errors: Temperature vector must have at least 2 nodes.");
    }
    if (L <= 0.0 || k <= 0.0) {
        throw std::invalid_argument("compute_midpoint_errors: L and k must be strictly positive.");
    }
    if (!std::isfinite(L) || !std::isfinite(k) || !std::isfinite(q) || !std::isfinite(T0) || !std::isfinite(TL)) {
        throw std::invalid_argument("compute_midpoint_errors: All physical parameters must be finite.");
    }

    const int N = static_cast<int>(T.size()) - 1;
    const double h = L / static_cast<double>(N);

    MidpointErrorResult res;
    res.x_mid.resize(N);
    res.T_fe_mid.resize(N);
    res.T_exact_mid.resize(N);
    res.errors.resize(N);
    res.max_error = 0.0;

    for (int e = 0; e < N; ++e) {
        const double x_m = (static_cast<double>(e) + 0.5) * h;
        const double t_fe = 0.5 * (T[e] + T[e + 1]);
        const double t_ex = exact_solution(x_m, L, k, q, T0, TL);
        const double err = std::abs(t_fe - t_ex);

        res.x_mid[e] = x_m;
        res.T_fe_mid[e] = t_fe;
        res.T_exact_mid[e] = t_ex;
        res.errors[e] = err;

        if (err > res.max_error) {
            res.max_error = err;
        }
    }

    return res;
}

double midpoint_error(const std::vector<double>& T, double L, double k, double q, double T0, double TL) {
    return compute_midpoint_errors(T, L, k, q, T0, TL).max_error;
}

SimulationResult run_for_N(double L, double A, double k, double q, double T0, double TL, int N) {
    if (N <= 0) {
        throw std::invalid_argument("run_for_N: Mesh element count N must be positive (> 0).");
    }

    GlobalSystem sys = assemble_global(L, A, k, q, N);
    apply_boundary_conditions(sys, T0, TL);
    std::vector<double> T = solve(sys);

    SimulationResult res;
    res.N = N;
    res.L = L;
    res.A = A;
    res.k = k;
    res.q = q;
    res.T0 = T0;
    res.TL = TL;
    res.h = L / static_cast<double>(N);

    // Compute nodal coordinates, exact solution, and errors
    const int num_nodes = N + 1;
    res.x_nodes.resize(num_nodes);
    res.T_fe = T;
    res.T_exact.resize(num_nodes);
    res.nodal_errors.resize(num_nodes);
    res.max_nodal_error = 0.0;

    for (int i = 0; i < num_nodes; ++i) {
        const double x_i = i * res.h;
        const double t_ex = exact_solution(x_i, L, k, q, T0, TL);
        const double err = std::abs(T[i] - t_ex);

        res.x_nodes[i] = x_i;
        res.T_exact[i] = t_ex;
        res.nodal_errors[i] = err;

        if (err > res.max_nodal_error) {
            res.max_nodal_error = err;
        }
    }

    // Compute midpoint values and errors
    MidpointErrorResult mp = compute_midpoint_errors(T, L, k, q, T0, TL);
    res.x_mid = mp.x_mid;
    res.T_fe_mid = mp.T_fe_mid;
    res.T_exact_mid = mp.T_exact_mid;
    res.midpoint_errors = mp.errors;
    res.max_midpoint_error = mp.max_error;

    return res;
}

std::string to_csv(const SimulationResult& res) {
    if (res.x_nodes.empty()) {
        throw std::invalid_argument("to_csv: SimulationResult contains no nodal data.");
    }

    std::ostringstream oss;
    oss << "x_m,T_fe_C,T_exact_C,error_C\n";
    oss.precision(8);
    oss << std::fixed;

    for (size_t i = 0; i < res.x_nodes.size(); ++i) {
        oss << res.x_nodes[i] << ","
            << res.T_fe[i] << ","
            << res.T_exact[i] << ","
            << res.nodal_errors[i] << "\n";
    }

    return oss.str();
}

void export_csv(const SimulationResult& res, const std::string& filepath) {
    if (filepath.empty()) {
        throw std::invalid_argument("export_csv: File path must not be empty.");
    }
    std::string content = to_csv(res);
    std::ofstream ofs(filepath);
    if (!ofs.is_open()) {
        throw std::runtime_error("export_csv: Failed to open file for writing: " + filepath);
    }
    ofs << content;
}

std::string convergence_to_csv(const std::vector<SimulationResult>& results) {
    if (results.empty()) {
        throw std::invalid_argument("convergence_to_csv: Results vector is empty.");
    }

    std::ostringstream oss;
    oss << "N,h_m,max_nodal_error_C,max_midpoint_error_C,observed_order\n";
    oss.precision(8);
    oss << std::fixed;

    for (size_t i = 0; i < results.size(); ++i) {
        const auto& r = results[i];
        double order = 0.0;
        if (i > 0 && r.max_midpoint_error > 0.0) {
            const double e_prev = results[i - 1].max_midpoint_error;
            const double h_ratio = results[i - 1].h / r.h;
            if (h_ratio > 1.0) {
                order = std::log(e_prev / r.max_midpoint_error) / std::log(h_ratio);
            }
        }

        oss << r.N << ","
            << r.h << ","
            << r.max_nodal_error << ","
            << r.max_midpoint_error << ",";
        if (i == 0) {
            oss << "N/A\n";
        } else {
            oss << order << "\n";
        }
    }

    return oss.str();
}

void export_convergence_csv(const std::vector<SimulationResult>& results, const std::string& filepath) {
    if (filepath.empty()) {
        throw std::invalid_argument("export_convergence_csv: File path must not be empty.");
    }
    std::string content = convergence_to_csv(results);
    std::ofstream ofs(filepath);
    if (!ofs.is_open()) {
        throw std::runtime_error("export_convergence_csv: Failed to open file for writing: " + filepath);
    }
    ofs << content;
}

static void append_json_array(std::ostringstream& oss, const std::string& key, const std::vector<double>& arr) {
    oss << "\"" << key << "\":[";
    for (size_t i = 0; i < arr.size(); ++i) {
        oss << arr[i];
        if (i + 1 < arr.size()) oss << ",";
    }
    oss << "]";
}

std::string to_json(const SimulationResult& res) {
    std::ostringstream oss;
    oss.precision(8);
    oss << std::fixed;
    oss << "{\"success\":true,"
        << "\"N\":" << res.N << ","
        << "\"L\":" << res.L << ","
        << "\"A\":" << res.A << ","
        << "\"k\":" << res.k << ","
        << "\"q\":" << res.q << ","
        << "\"T0\":" << res.T0 << ","
        << "\"TL\":" << res.TL << ","
        << "\"h\":" << res.h << ","
        << "\"max_nodal_error\":" << res.max_nodal_error << ","
        << "\"max_midpoint_error\":" << res.max_midpoint_error << ",";

    append_json_array(oss, "x_nodes", res.x_nodes);
    oss << ",";
    append_json_array(oss, "T_fe", res.T_fe);
    oss << ",";
    append_json_array(oss, "T_exact", res.T_exact);
    oss << ",";
    append_json_array(oss, "nodal_errors", res.nodal_errors);
    oss << ",";
    append_json_array(oss, "x_mid", res.x_mid);
    oss << ",";
    append_json_array(oss, "T_fe_mid", res.T_fe_mid);
    oss << ",";
    append_json_array(oss, "T_exact_mid", res.T_exact_mid);
    oss << ",";
    append_json_array(oss, "midpoint_errors", res.midpoint_errors);
    oss << "}";

    return oss.str();
}

std::string convergence_to_json(const std::vector<SimulationResult>& results) {
    std::ostringstream oss;
    oss.precision(8);
    oss << std::fixed;
    oss << "{\"success\":true,\"meshes\":[";

    for (size_t i = 0; i < results.size(); ++i) {
        const auto& r = results[i];
        double order = 0.0;
        bool has_order = false;
        if (i > 0 && r.max_midpoint_error > 0.0) {
            const double e_prev = results[i - 1].max_midpoint_error;
            const double h_ratio = results[i - 1].h / r.h;
            if (h_ratio > 1.0) {
                order = std::log(e_prev / r.max_midpoint_error) / std::log(h_ratio);
                has_order = true;
            }
        }

        oss << "{\"N\":" << r.N
            << ",\"h\":" << r.h
            << ",\"max_nodal_error\":" << r.max_nodal_error
            << ",\"max_midpoint_error\":" << r.max_midpoint_error
            << ",\"observed_order\":";
        if (has_order) {
            oss << order;
        } else {
            oss << "null";
        }
        oss << "}";
        if (i + 1 < results.size()) oss << ",";
    }

    oss << "]}";
    return oss.str();
}

} // namespace thermo
