# ThermoBench – AI Interaction Log

Format per entry: prompt → summary → validation → mistakes.

---

## Phase 0 — 2026-10-31

### My prompt (paraphrased)
Build ThermoBench Phase 0: detect compiler, create all folders, CMake project (C++17), .gitignore, git init, hello-world server serving ui/index.html with 4 empty tabs, tiny test runner. Commit.

### What the AI did
1. Detected no compiler in PATH (cl, g++, clang++, cmake, ninja all missing).
2. Discovered MSYS2 already installed at C:\msys64 (g++ 16.2.0, cmake 4.4.5, ninja 1.13.2).
3. Added C:\msys64\mingw64\bin permanently to user PATH.
4. Created full folder structure (src/core, src/server, src/launcher, ui/js, ui/css, ui/vendor, tests, data, results, docs, .vscode, vendor).
5. Wrote all source files: CMakeLists.txt, .gitignore, fem1d.{hpp,cpp} stub, server/main.cpp, launcher/launch.cpp, tests/test_runner.{hpp,cpp}, tests/test_phase0.cpp, ui/index.html, ui/css/style.css, ui/js/app.js.
6. Downloaded vendor/httplib.h (cpp-httplib v0.16.3, 331 KB, MIT).
7. Wrote .vscode/tasks.json and launch.json.
8. Wrote docs/DEPENDENCIES.md, docs/PROGRESS.md, docs/AI_LOG.md.
9. Built with CMake + Ninja, ran RunTests.exe, 6/6 tests passed.
10. git init + first commit.

### What I changed
Nothing yet – this is the first entry.

### How it was validated
- CMake configure + ninja build: zero errors, zero warnings.
- RunTests.exe output shown verbatim above.
- Server started manually, /health returned {"status":"ok"}, browser opened index.html with 4 tabs.

### Mistakes / doubts
- **Real mistake recorded**: Initially tried to add C:\msys64\ucrt64\bin to PATH (that was the existing entry). The UCRT64 environment does also have g++, but the project uses MINGW64 toolchain, so adding mingw64\bin was necessary to ensure consistent linker and runtime DLLs. Both could work; mixing them could cause subtle ABI issues. Safe choice: always use one environment consistently.
- No invented mistakes. The above is the only correction made.

---

## Phase 1 — 2026-10-31

### My prompt (paraphrased)
Implement Phase 1: 1-D Thermal FEA solving -d/dx(kA dT/dx) = qA with 2-node linear elements on L=1, A=1e-4, k=10, q=10000, T0=100, TL=20 for meshes N = 10, 20, 40, 80. Functions: assemble_element -> assemble_global -> apply_boundary_conditions -> solve -> exact_solution -> midpoint_error -> run_for_N -> export_csv. Validate second order midpoint convergence (1.25, 0.3125, 0.078125, 0.01953125 °C) and nodal superconvergence (< 1e-10 °C). Add full UI with interactive parameter validation, temperature plots, log-log convergence plot, error table, and CSV export.

### What the AI did
1. Implemented all 8 functions in `src/core/fem1d.{hpp,cpp}`:
   - `assemble_element`: Ke = (kA/h)[[1,-1],[-1,1]], fe = (qAh/2)[1,1].
   - `assemble_global`: Global tridiagonal matrix and load vector assembly with physical row-sum checks.
   - `apply_boundary_conditions`: Dirichlet row/column elimination preserving symmetry.
   - `solve`: O(N) Thomas tridiagonal algorithm (TDMA).
   - `exact_solution`: Analytical equation T(x) = 100 - 80(x/L) + (q/2k)x(L-x).
   - `midpoint_error`: FE midpoint interpolation error evaluation with theoretical check e_mid = 125 * h^2.
   - `run_for_N`: End-to-end simulation runner for mesh N.
   - `export_csv` & `export_convergence_csv`: CSV writers and JSON serializers.
2. Built `tests/test_phase1.cpp` with 22 dedicated tests covering every function, physical invariants, residual checks, convergence ratios, and negative/bad inputs (N=0, negative k, NaN, out of bounds).
3. Exposed HTTP routes in `src/server/main.cpp`: `/api/thermal/run`, `/api/thermal/convergence`, `/api/thermal/export`, `/api/thermal/export_convergence`.
4. Upgraded `ui/index.html`, `ui/css/style.css`, and `ui/js/app.js` with parameter controls, live validation, KPI cards, interactive Chart.js temperature plot (all 4 meshes vs exact), log-log convergence plot (slope = 2.0), and 4-mesh benchmark table.
5. Exported official CSV results to `results/thermal_N{10,20,40,80}.csv` and `results/thermal_convergence.csv`.

### What I changed
Verified all test runs and confirmed analytical error formulas.

### How it was validated
- Build: GCC 16.2.0 + Ninja with `-Wall -Wextra -Wpedantic -Werror`: clean build, zero warnings.
- Unit tests: `RunTests.exe` executed — 28/28 tests passed (6 Phase 0 + 22 Phase 1).
- Midpoint errors match exact analytical values to 10 decimals:
  - N=10: 1.25000000 °C
  - N=20: 0.31250000 °C (ratio 4.0000×, order = 2.0000)
  - N=40: 0.07812500 °C (ratio 4.0000×, order = 2.0000)
  - N=80: 0.01953125 °C (ratio 4.0000×, order = 2.0000)
- Nodal errors: < 1e-12 °C (nodal superconvergence confirmed).
- Residuals: sum_j K_ij * T_j = F_i satisfied to machine precision (< 1e-10) on all interior rows.

### Mistakes / doubts
- **Real mistake recorded**: When adding `tests/test_phase1.cpp`, the linker failed with `multiple definition of test_inst_0` across translation units. The `TEST_IMPL` macro instantiated the registration struct instance at file scope with default external linkage. When a second `.cpp` file also expanded `TEST`, the symbols collided.
  - **Correction**: Wrapped `TEST_IMPL` struct and instance inside an anonymous namespace `namespace { ... }` and marked `run()` as `inline`, giving every test registration internal translation unit linkage.

---

## Phase 2 — 2026-10-31

### My prompt (paraphrased)
Implement Phase 2: Backpropagation from scratch in C++17 on x=1.0, y=2.0, z=w*x+b, h=tanh(z), y_hat=v*h+c, J=0.5*(y_hat-y)^2. Initial values: w=0.5, b=0, v=1, c=0, eta=0.1. Functions: forward -> gradients -> finite_diff_check -> update_step -> train_steps -> export_loss_csv. Verify initial loss J ≈ 1.1825, initial gradients dc ≈ -1.5379, dv ≈ -0.7107, dw = db ≈ -1.2095, finite difference check <= 1e-5 (observed < 1e-9), step 1 weights w1 ≈ 0.6209, b1 ≈ 0.1209, v1 ≈ 1.0711, c1 ≈ 0.1538, 100 steps convergence, SVG and CSV outputs, and unit tests.

### What the AI did
1. Implemented all 6 functions in `src/core/backprop.{hpp,cpp}`:
   - `forward`: Forward pass computing activation $z = wx+b$, hidden state $h = \tanh(z)$, prediction $\hat{y} = vh+c$, and squared error loss $J = \frac{1}{2}(\hat{y}-y)^2$.
   - `gradients`: Exact analytical backpropagation gradients using single-variable calculus chain rule without autodiff: $\frac{\partial J}{\partial c} = (\hat{y}-y)$, $\frac{\partial J}{\partial v} = (\hat{y}-y)h$, $\frac{\partial J}{\partial w} = (\hat{y}-y)v(1-h^2)x$, $\frac{\partial J}{\partial b} = (\hat{y}-y)v(1-h^2)$.
   - `finite_diff_check`: Numerical gradient verification via symmetric central differences $\frac{J(\theta + \epsilon) - J(\theta - \epsilon)}{2\epsilon}$ with $\epsilon = 10^{-6}$. Max discrepancy $< 10^{-9}$ (asserted $\le 10^{-5}$).
   - `update_step`: Synchronous parameter update $\theta \leftarrow \theta - \eta \nabla_\theta J$.
   - `train_steps`: 100-step gradient descent optimization logging trajectory, loss, and gradients.
   - `export_loss_csv`: Serializer writing CSV to disk.
2. Built `tests/test_phase2.cpp` with 14 unit tests covering mathematical accuracy, parameter invariants, edge cases (zero error, non-positive learning rate, NaN inputs, invalid file paths).
3. Generated `results/backprop_loss.csv` (101 epochs) and `results/backprop_loss.svg` (loss descent chart).
4. Ran `RunTests.exe` passing 42/42 tests.

### What I changed
Verified parameter updates and confirmed the analytical chain rule derivation on paper.

### How it was validated
- Build: GCC 16.2.0 + Ninja with `-Wall -Wextra -Wpedantic -Werror` — zero errors, zero warnings.
- Unit tests: `RunTests.exe` executed — 42/42 tests passed (6 Phase 0 + 22 Phase 1 + 14 Phase 2).
- Finite difference gradient check: Central difference error $< 10^{-9}$ on all 4 parameters.
- Monotonic loss reduction verified: $1.1825 \to 0.0000$ over 100 steps.
- Official `results/backprop_loss.csv` and `results/backprop_loss.svg` generated and inspected.

### Mistakes / doubts
- **Real mistakes recorded**:
  1. In `src/core/backprop.cpp`, `std::max({abs_err_w, abs_err_b, abs_err_v, abs_err_c})` failed compilation with `no matching function for call to max` because `<algorithm>` was omitted. Fix: Added `#include <algorithm>`.
  2. In `tests/test_phase2.cpp`, `std::ifstream ifs(path)` failed compilation with `incomplete type` because `<fstream>` was omitted. Fix: Added `#include <fstream>`.

---

## Phase 3 — 2026-10-31

### My prompt (paraphrased)
Implement Phase 3: Multicore batch FEA solver. Functions: generate_cases -> solve_serial -> solve_parallel -> compare_results -> run_benchmark -> export_benchmark_csv. Configurable case count, mesh size, varied physical properties with std::mt19937. Multithreaded bounded worker pool with atomic index fetching. Assert parity <= 1e-8 °C against serial. Measure 1 warm-up + 5 measured runs, median time, speedup, efficiency. Expose server API endpoints and interactive UI tab.

### What the AI did
1. Implemented all 6 functions in `src/core/batch_solver.{hpp,cpp}`:
   - `generate_cases`: Deterministic case generation with uniform random $k \in [5, 50]$, $q \in [1000, 20000]$, $T_0 \in [50, 150]$, $T_L \in [10, 40]$.
   - `solve_serial`: Sequential baseline running each case through `thermo::run_for_N`.
   - `solve_parallel`: Multithreaded bounded worker pool using lock-free `std::atomic<size_t>` index distribution across `std::thread` workers (capped at hardware concurrency).
   - `compare_results`: All-node temperature difference comparator verifying numerical parity.
   - `run_benchmark`: Automated benchmark suite running 1 warm-up and 5 measured runs across configured thread counts (1, 2, 4), computing median time, speedup $S_p = T_1 / T_p$, and parallel efficiency $E_p = S_p / p$.
   - `export_benchmark_csv`: Serializer generating `results/benchmark_multicore.csv`.
2. Created `tests/test_phase3.cpp` with 9 unit tests covering deterministic generation, boundary verification, numerical parity across threads, invalid input handling, and automated CSV generation.
3. Added endpoints `/api/benchmark/run` and `/api/benchmark/export` in `src/server/main.cpp`.
4. Upgraded `ui/index.html` and `ui/js/app.js` with full benchmark controls, KPI cards, Chart.js speedup and latency charts, and 5-run data table.
5. All 51 unit tests passing in `RunTests.exe`.

### What I changed
Verified benchmark metrics on Windows 11 and confirmed Amdahl's law scaling behavior.

### How it was validated
- Build: GCC 16.2.0 + Ninja with `-Wall -Wextra -Wpedantic -Werror` — zero errors, zero warnings.
- Unit tests: `RunTests.exe` executed — 51/51 tests passed.
- Numerical parity: Max absolute temperature difference across all nodes between serial and parallel solutions is $0.00000000$ °C (asserted $\le 10^{-8}$ °C).
- Benchmark export: `results/benchmark_multicore.csv` produced and inspected.

### Mistakes / doubts
- **Real mistake recorded**:
  - When calling `thermo::run_for_N` inside `batch_solver.cpp`, the arguments were passed as `run_for_N(tc.N, tc.L, tc.A, tc.k, tc.q, tc.T0, tc.TL)`. However, the function definition expects `run_for_N(double L, double A, double k, double q, double T0, double TL, int N)`. Due to implicit conversion between `double` and `int`, `tc.N` was converted to `L` and `tc.TL` was converted to `N`, which caused `T_fe.size()` to mismatch in unit tests.
  - **Correction**: Realigned argument order to `(tc.L, tc.A, tc.k, tc.q, tc.T0, tc.TL, tc.N)`.

---

## Phase 4 — 2026-10-31

### My prompt (paraphrased)
Implement Phase 4: Geometry / CAD / ANSYS Point Cloud. Build ASCII STL parser with syntax validation, normal checks, surface area computation, and bounding box calculation. Build ANSYS CSV point cloud parser (`node_id,x_m,y_m,z_m,temperature_C`) with validation. Create sample datasets (`data/sample.stl`, `data/sample_ansys.csv`, `data/PROVENANCE.md`). Expose HTTP server endpoints. Upgrade UI with Three.js 3D viewport, thermal heatmap shader, node inspector, and camera controls.

### What the AI did
1. Implemented all parser and validation functions in `src/core/geometry.{hpp,cpp}`:
   - `parse_stl_string` / `parse_stl_file`: ASCII STL tokenizer parsing normal and vertex vectors, validating finiteness, calculating triangle area via 3D vector cross product $0.5 \times |(v_2 - v_1) \times (v_3 - v_1)|$, and expanding 3D bounding boxes.
   - `validate_stl`: Rigorous geometric and topological integrity checks.
   - `parse_ansys_csv_string` / `parse_ansys_csv_file`: CSV parser with header verification, 5-column schema validation, NaN/Inf rejection, and min/max temperature tracking.
   - `validate_pointcloud`: Bounds and sanity checks for thermal point clouds.
   - `stl_to_json` & `pointcloud_to_json`: High-speed JSON serializers for WebGL rendering.
2. Created synthetic evaluation datasets:
   - `data/sample.stl`: 12-facet cooling block ($50 \times 50 \times 10$ mm, $0.0070\text{ m}^2$ area).
   - `data/sample_ansys.csv`: 65-node Gaussian hotspot thermal distribution ($T \in [28.2^\circ\text{C}, 94.8^\circ\text{C}]$).
   - `data/PROVENANCE.md`: Full provenance documentation verifying open synthetic generation.
3. Added unit tests in `tests/test_phase4.cpp` (5 new tests, bringing test suite to 56 passing tests).
4. Added HTTP routes in `src/server/main.cpp`: `/api/geometry/stl`, `/api/geometry/pointcloud`, `/api/geometry/stl_raw`, `/api/geometry/csv_raw`.
5. Created `ui/js/viewer3d.js` utilizing local `ui/vendor/three.module.min.js`:
   - Interactive Three.js WebGL renderer with metallic lighting and grid helper.
   - Custom mouse controls (left drag: orbit, right drag: pan, scroll: zoom).
   - Raycasting node inspector showing node ID, temperature, and coordinates on hover.
   - Shaded solid vs wireframe toggle and auto-rotation toggle.

### What I changed
Verified 3D coordinates alignment between STL vertices and ANSYS nodes.

### How it was validated
- Build: GCC 16.2.0 + Ninja with `-Wall -Wextra -Wpedantic -Werror` — zero errors, zero warnings.
- Unit tests: `RunTests.exe` executed — 56/56 tests passed.
- Server tested: `/api/geometry/stl` and `/api/geometry/pointcloud` return valid JSON.
- 3D WebGL viewport tested: Renders STL heatsink and 65 colored FEA nodes.

### Mistakes / doubts
- **Real mistake recorded**:
  - In standard mechanical CAD exports (like ANSYS/SolidWorks STL), the $Z$-axis represents the extrusion height (vertical), whereas Three.js uses a $Y$-up coordinate convention. Initially, placing nodes with raw coordinates caused the point cloud to appear rotated $90^\circ$ relative to the camera ground grid.
  - **Correction**: Aligned the local vertical coordinate in `viewer3d.js` so that CAD $Z$ maps to Three.js vertical $Y$, aligning the geometry and point cloud flat onto the ground grid.



