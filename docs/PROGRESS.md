# ThermoBench – Progress Log

## Phase 0 — Setup  (2026-10-31)
**Status:** ✅ COMPLETE

### What was built
| Item | Done |
|------|------|
| Folder structure | ✅ |
| CMakeLists.txt (C++17, Ninja, MSYS2/GCC) | ✅ |
| .gitignore | ✅ |
| git init + first commit | ✅ |
| src/core/fem1d.{hpp,cpp} stub | ✅ |
| src/server/main.cpp (hello-world + /health) | ✅ |
| src/launcher/launch.cpp | ✅ |
| tests/test_runner.{hpp,cpp} | ✅ |
| tests/test_phase0.cpp (6 tests, all pass) | ✅ |
| ui/index.html (4 tabs) | ✅ |
| ui/css/style.css (dark-orange theme) | ✅ |
| ui/js/app.js | ✅ |
| vendor/httplib.h (v0.16.3) | ✅ |
| .vscode/tasks.json + launch.json | ✅ |
| docs/DEPENDENCIES.md | ✅ |
| docs/AI_LOG.md | ✅ |

### Test results (real run)
```
=== ThermoBench Test Runner ===
  [ RUN ] framework: ASSERT true
  [ OK  ] framework: ASSERT true
  [ RUN ] framework: ASSERT_NEAR passes
  [ OK  ] framework: ASSERT_NEAR passes
  [ RUN ] framework: ASSERT_NEAR fails correctly
  [ OK  ] framework: ASSERT_NEAR fails correctly
  [ RUN ] framework: ASSERT_THROWS passes
  [ OK  ] framework: ASSERT_THROWS passes
  [ RUN ] core: version() returns non-empty string
  [ OK  ] core: version() returns non-empty string
  [ RUN ] core: version() contains 'ThermoBench'
  [ OK  ] core: version() contains 'ThermoBench'

Result: 6 passed, 0 failed.
```

---

## Phase 1 — Thermal FEA  (2026-10-31)
**Status:** ✅ COMPLETE

### What was built
| Function | Description | Test Status |
|----------|-------------|-------------|
| `assemble_element` | 2-node linear stiffness matrix Ke and load vector fe | ✅ PASS (analytical formulas + bad input) |
| `assemble_global` | Global K and F assembly for N uniform elements | ✅ PASS (symmetry, conservation, tridiagonal) |
| `apply_boundary_conditions` | Dirichlet BCs via row/column elimination | ✅ PASS (T0=100, TL=20, decoupled rows, RHS mod) |
| `solve` | Thomas algorithm (TDMA) for tridiagonal system | ✅ PASS (residual K*T=F < 1e-10, O(N) complexity) |
| `exact_solution` | Analytical benchmark: T(x)=100-80x+(q/2k)x(1-x) | ✅ PASS (bounds, midpoints, peaks) |
| `midpoint_error` | FE midpoint interpolation error evaluation | ✅ PASS (1.25, 0.3125, 0.078125, 0.01953125 °C) |
| `run_for_N` | Full pipeline execution for mesh size N | ✅ PASS (N=10, 20, 40, 80 end-to-end) |
| `export_csv` | CSV serializers for per-mesh and convergence data | ✅ PASS (schema, line count, file output) |

### Real Test Output (RunTests.exe)
```
=== ThermoBench Test Runner ===
  [ RUN ] Phase 1 - assemble_element: values match analytical formulas
  [ OK  ] Phase 1 - assemble_element: values match analytical formulas
  [ RUN ] Phase 1 - assemble_element: negative/zero inputs throw invalid_argument
  [ OK  ] Phase 1 - assemble_element: negative/zero inputs throw invalid_argument
  [ RUN ] Phase 1 - assemble_element: NaN / Inf inputs throw invalid_argument
  [ OK  ] Phase 1 - assemble_element: NaN / Inf inputs throw invalid_argument
  [ RUN ] Phase 1 - assemble_global: dimensions, symmetry, tridiagonal and row sums
  [ OK  ] Phase 1 - assemble_global: dimensions, symmetry, tridiagonal and row sums
  [ RUN ] Phase 1 - assemble_global: invalid N and negative params throw invalid_argument
  [ OK  ] Phase 1 - assemble_global: invalid N and negative params throw invalid_argument
  [ RUN ] Phase 1 - apply_boundary_conditions: row/column elimination and RHS modification
  [ OK  ] Phase 1 - apply_boundary_conditions: row/column elimination and RHS modification
  [ RUN ] Phase 1 - apply_boundary_conditions: invalid inputs throw invalid_argument
  [ OK  ] Phase 1 - apply_boundary_conditions: invalid inputs throw invalid_argument
  [ RUN ] Phase 1 - solve: Thomas solver satisfies K*T = F to machine precision on interior
  [ OK  ] Phase 1 - solve: Thomas solver satisfies K*T = F to machine precision on interior
  [ RUN ] Phase 1 - solve: minimal mesh N=2 (1 interior unknown)
  [ OK  ] Phase 1 - solve: minimal mesh N=2 (1 interior unknown)
  [ RUN ] Phase 1 - solve: invalid input throws invalid_argument
  [ OK  ] Phase 1 - solve: invalid input throws invalid_argument
  [ RUN ] Phase 1 - exact_solution: matches known analytical benchmark points
  [ OK  ] Phase 1 - exact_solution: matches known analytical benchmark points
  [ RUN ] Phase 1 - exact_solution: out of bounds and non-finite inputs throw invalid_argument
  [ OK  ] Phase 1 - exact_solution: out of bounds and non-finite inputs throw invalid_argument
  [ RUN ] Phase 1 - midpoint_error: matches (q/(2k))*(h/2)^2 for N=10, 20, 40, 80
  [ OK  ] Phase 1 - midpoint_error: matches (q/(2k))*(h/2)^2 for N=10, 20, 40, 80
  [ RUN ] Phase 1 - midpoint_error: rate of convergence is 2.0 (drops 4x per doubling)
  [ OK  ] Phase 1 - midpoint_error: rate of convergence is 2.0 (drops 4x per doubling)
  [ RUN ] Phase 1 - midpoint_error: invalid inputs throw invalid_argument
  [ OK  ] Phase 1 - midpoint_error: invalid inputs throw invalid_argument
  [ RUN ] Phase 1 - run_for_N: end-to-end pipeline execution for N=10, 20, 40, 80
  [ OK  ] Phase 1 - run_for_N: end-to-end pipeline execution for N=10, 20, 40, 80
  [ RUN ] Phase 1 - run_for_N: invalid parameters throw invalid_argument
  [ OK  ] Phase 1 - run_for_N: invalid parameters throw invalid_argument
  [ RUN ] Phase 1 - export_csv: to_csv produces valid schema and expected rows
  [ OK  ] Phase 1 - export_csv: to_csv produces valid schema and expected rows
  [ RUN ] Phase 1 - export_csv: exports to file and content matches
  [ OK  ] Phase 1 - export_csv: exports to file and content matches
  [ RUN ] Phase 1 - export_csv: convergence CSV produces valid rate table
  [ OK  ] Phase 1 - export_csv: convergence CSV produces valid rate table
  [ RUN ] Phase 1 - export_csv: generate official benchmark CSV files in results/
  [ OK  ] Phase 1 - export_csv: generate official benchmark CSV files in results/
  [ RUN ] Phase 1 - export_csv: invalid inputs throw invalid_argument
  [ OK  ] Phase 1 - export_csv: invalid inputs throw invalid_argument
  [ RUN ] framework: ASSERT true
  [ OK  ] framework: ASSERT true
  [ RUN ] framework: ASSERT_NEAR passes
  [ OK  ] framework: ASSERT_NEAR passes
  [ RUN ] framework: ASSERT_NEAR fails correctly
  [ OK  ] framework: ASSERT_NEAR fails correctly
  [ RUN ] framework: ASSERT_THROWS passes
  [ OK  ] framework: ASSERT_THROWS passes
  [ RUN ] core: version() returns non-empty string
  [ OK  ] core: version() returns non-empty string
  [ RUN ] core: version() contains 'ThermoBench'
  [ OK  ] core: version() contains 'ThermoBench'

Result: 28 passed, 0 failed.
```

### Convergence Verification Table
| Mesh N | Element Size h [m] | Max Nodal Error [°C] | Max Midpoint Error [°C] | Theoretical Error [°C] | Ratio | Observed Order p |
|--------|-------------------|----------------------|-------------------------|------------------------|-------|------------------|
| 10 | 0.1000 | 0.00000000 | 1.25000000 | 1.25000000 | — | — |
| 20 | 0.0500 | 0.00000000 | 0.31250000 | 0.31250000 | 4.0000× | 2.0000 |
| 40 | 0.0250 | 0.00000000 | 0.07812500 | 0.07812500 | 4.0000× | 2.0000 |
| 80 | 0.0125 | 0.00000000 | 0.01953125 | 0.01953125 | 4.0000× | 2.0000 |

---

## Phase 2 — Backpropagation  (2026-10-31)
**Status:** ✅ COMPLETE

### What was built
| Function | Description | Test Status |
|----------|-------------|-------------|
| `forward` | Forward inference: z = w*x + b, h = tanh(z), y_hat = v*h + c, loss = 0.5*(y_hat - y)^2 | ✅ PASS (h~0.4621, y_hat~0.4621, J~1.1825) |
| `gradients` | Analytical chain rule: dc, dv, dw, db without autodiff | ✅ PASS (dc~-1.5379, dv~-0.7107, dw=db~-1.2095) |
| `finite_diff_check` | Central finite difference check with eps = 1e-6 | ✅ PASS (max abs error <= 1e-5, verified < 1e-9) |
| `update_step` | Simultaneous parameter update: theta - lr * grad | ✅ PASS (w~0.6209, b~0.1209, v~1.0711, c~0.1538) |
| `train_steps` | 100-step gradient descent loop with convergence tracking | ✅ PASS (monotonic descent, final loss < 0.05) |
| `export_loss_csv` | Serializes training history to CSV and writes results/backprop_loss.csv | ✅ PASS (102 lines, header + 101 records verified) |

### Real Test Output (RunTests.exe)
```
=== ThermoBench Test Runner ===
  [ RUN ] Phase 2 - forward: verifies initial state against analytical values
  [ OK  ] Phase 2 - forward: verifies initial state against analytical values
  [ RUN ] Phase 2 - forward: non-finite inputs throw invalid_argument
  [ OK  ] Phase 2 - forward: non-finite inputs throw invalid_argument
  [ RUN ] Phase 2 - gradients: analytical chain rule matches brief values
  [ OK  ] Phase 2 - gradients: analytical chain rule matches brief values
  [ RUN ] Phase 2 - gradients: zero error produces zero gradients
  [ OK  ] Phase 2 - gradients: zero error produces zero gradients
  [ RUN ] Phase 2 - gradients: non-finite inputs throw invalid_argument
  [ OK  ] Phase 2 - gradients: non-finite inputs throw invalid_argument
  [ RUN ] Phase 2 - finite_diff_check: central difference error <= 1e-5 for all four parameters
  [ OK  ] Phase 2 - finite_diff_check: central difference error <= 1e-5 for all four parameters
  [ RUN ] Phase 2 - finite_diff_check: holds across arbitrary non-zero parameters
  [ OK  ] Phase 2 - finite_diff_check: holds across arbitrary non-zero parameters
  [ RUN ] Phase 2 - finite_diff_check: non-positive eps throws invalid_argument
  [ OK  ] Phase 2 - finite_diff_check: non-positive eps throws invalid_argument
  [ RUN ] Phase 2 - update_step: simultaneous update matches brief values exactly
  [ OK  ] Phase 2 - update_step: simultaneous update matches brief values exactly
  [ RUN ] Phase 2 - update_step: non-positive or non-finite lr throws invalid_argument
  [ OK  ] Phase 2 - update_step: non-positive or non-finite lr throws invalid_argument
  [ RUN ] Phase 2 - train_steps: 100 steps of gradient descent convergence
  [ OK  ] Phase 2 - train_steps: 100 steps of gradient descent convergence
  [ RUN ] Phase 2 - train_steps: invalid steps or lr throws invalid_argument
  [ OK  ] Phase 2 - train_steps: invalid steps or lr throws invalid_argument
  [ RUN ] Phase 2 - export_loss_csv: serializes and writes official results/backprop_loss.csv
  [ OK  ] Phase 2 - export_loss_csv: serializes and writes official results/backprop_loss.csv
  [ RUN ] Phase 2 - export_loss_csv: invalid arguments throw invalid_argument
  [ OK  ] Phase 2 - export_loss_csv: invalid arguments throw invalid_argument
  (All 28 Phase 1 + Phase 0 tests also run and pass)

Result: 42 passed, 0 failed.
```

---

## Phase 3 — Multicore Batch Solver (2026-10-31)
**Status:** ✅ COMPLETE

### What was built
| Function | Description | Test Status |
|----------|-------------|-------------|
| `generate_cases` | Deterministic generation of test cases using `std::mt19937` with physical $k \in [5, 50]$, $q \in [1000, 20000]$ | ✅ PASS |
| `solve_serial` | Sequential baseline solver using `fem1d::run_for_N` | ✅ PASS |
| `solve_parallel` | Multithreaded bounded worker pool with lock-free atomic task distribution | ✅ PASS |
| `compare_results` | Maximum absolute temperature difference validator across all nodes and cases | ✅ PASS (parity $\le 10^{-8}$ °C) |
| `run_benchmark` | Automated 1 warm-up + 5 measured runs suite computing median time, speedup $S_p$, efficiency $E_p$ | ✅ PASS |
| `export_benchmark_csv` | CSV serializer writing `results/benchmark_multicore.csv` | ✅ PASS |

### Real Test Output (RunTests.exe)
```
=== ThermoBench Test Runner ===
  [ RUN ] Phase 3 - generate_cases: deterministic output for same seed, varied k, q > 0
  [ OK  ] Phase 3 - generate_cases: deterministic output for same seed, varied k, q > 0
  [ RUN ] Phase 3 - generate_cases: zero count or N < 2 throws invalid_argument
  [ OK  ] Phase 3 - generate_cases: zero count or N < 2 throws invalid_argument
  [ RUN ] Phase 3 - solve_serial: solves batch accurately, boundaries match T0 and TL
  [ OK  ] Phase 3 - solve_serial: solves batch accurately, boundaries match T0 and TL
  [ RUN ] Phase 3 - solve_serial: empty test cases throws invalid_argument
  [ OK  ] Phase 3 - solve_serial: empty test cases throws invalid_argument
  [ RUN ] Phase 3 - solve_parallel: numerical parity across 1, 2, 4 threads within 1e-8 C
  [ OK  ] Phase 3 - solve_parallel: numerical parity across 1, 2, 4 threads within 1e-8 C
  [ RUN ] Phase 3 - solve_parallel: zero threads or empty list throws invalid_argument
  [ OK  ] Phase 3 - solve_parallel: zero threads or empty list throws invalid_argument
  [ RUN ] Phase 3 - compare_results: zero diff for identical, detects perturbations
  [ OK  ] Phase 3 - compare_results: zero diff for identical, detects perturbations
  [ RUN ] Phase 3 - benchmark and CSV export: verified warm-up, 5 runs median, speedup, CSV
  [ OK  ] Phase 3 - benchmark and CSV export: verified warm-up, 5 runs median, speedup, CSV
  [ RUN ] Phase 3 - generate official benchmark_multicore.csv
  [ OK  ] Phase 3 - generate official benchmark_multicore.csv

Result: 51 passed, 0 failed.
```

### Benchmark Scaling Results (results/benchmark_multicore.csv)
- Cases: 200, Mesh elements per case: $N=40$ (8,200 total nodal thermal solutions per run).
- 1 Thread (Baseline): Median $5.27$ ms, Speedup $1.00\times$, Efficiency $100\%$
- 2 Threads: Median $4.60$ ms, Speedup $1.15\times$, Efficiency $57.3\%$
- 4 Threads: Median $3.42$ ms, Speedup $1.54\times$, Efficiency $38.6\%$
- Numerical Parity: $0.00000000$ °C max difference ($\le 10^{-8}$ °C).

---

## Phase 4 — Geometry / CAD / ANSYS Point Cloud (2026-10-31)
**Status:** ✅ COMPLETE

### What was built
| Function | Description | Test Status |
|----------|-------------|-------------|
| `parse_stl_string` / `parse_stl_file` | Robust ASCII STL parser extracting vertices, facet normals, triangle counts, bounding boxes, and surface areas | ✅ PASS |
| `validate_stl` | Geometric integrity checker detecting non-finite numbers, degenerate boxes, and syntax errors | ✅ PASS |
| `parse_ansys_csv_string` / `parse_ansys_csv_file` | ANSYS FEA point cloud parser (`node_id,x_m,y_m,z_m,temperature_C`) | ✅ PASS |
| `validate_pointcloud` | Validates point cloud bounds, min/max temperatures, and finite coordinates | ✅ PASS |
| `stl_to_json` | Compact JSON serializer for CAD geometry rendering in WebGL | ✅ PASS |
| `pointcloud_to_json` | Compact JSON serializer for FEA nodal temperatures | ✅ PASS |

### Real Test Output (RunTests.exe)
```
=== ThermoBench Test Runner ===
  [ RUN ] Phase 4 - parse_stl: parses sample.stl with 12 triangles and exact bbox
  [ OK  ] Phase 4 - parse_stl: parses sample.stl with 12 triangles and exact bbox
  [ RUN ] Phase 4 - parse_stl: rejects invalid header, truncated content, and NaN coordinates
  [ OK  ] Phase 4 - parse_stl: rejects invalid header, truncated content, and NaN coordinates
  [ RUN ] Phase 4 - parse_ansys_csv: parses sample_ansys.csv and extracts 65 thermal nodes
  [ OK  ] Phase 4 - parse_ansys_csv: parses sample_ansys.csv and extracts 65 thermal nodes
  [ RUN ] Phase 4 - parse_ansys_csv: rejects bad header, missing columns, and non-numeric values
  [ OK  ] Phase 4 - parse_ansys_csv: rejects bad header, missing columns, and non-numeric values
  [ RUN ] Phase 4 - serializers: stl_to_json and pointcloud_to_json produce valid schemas
  [ OK  ] Phase 4 - serializers: stl_to_json and pointcloud_to_json produce valid schemas

Result: 56 passed, 0 failed.
```

### Datasets Created
- `data/sample.stl`: ASCII STL cooling block ($50\text{ mm} \times 50\text{ mm} \times 10\text{ mm}$, 12 facets, $0.0070\text{ m}^2$ area).
- `data/sample_ansys.csv`: 65-node synthetic ANSYS FEA thermal point cloud ($T \in [28.2^\circ\text{C}, 94.8^\circ\text{C}]$).
- `data/PROVENANCE.md`: Mathematical specification, coordinate frame, and confirmation that no proprietary/NDA data is used.

### Web UI
- `ui/index.html` & `ui/js/viewer3d.js`: Three.js interactive 3D WebGL viewport with solid/wireframe toggle, thermal colormap, node hover inspector, and camera orbit/pan/zoom controls.

---

## Phase 5 — Embedded Hardware Mocks  (2026-10-31)
**Status:** ✅ COMPLETE

### What was built
| Module | Description | Test Status |
|--------|-------------|-------------|
| `MockDevice` | Simulates MCU/FPGA/JTAG with connect/disconnect/telemetry/fault | ✅ PASS |
| `EmbeddedManager` | Singleton device registry + mutex-safe API | ✅ PASS |
| `trigger_timeout` | Injects ERR_TIMEOUT fault with counter | ✅ PASS |
| `recover_device` | Bus re-enumeration and frame-sync recovery | ✅ PASS |
| JSON serializers | `devices_to_json`, `telemetry_to_json`, `all_telemetry_to_json` | ✅ PASS |
| REST endpoints | `/api/hardware/*` wired into HTTP server | ✅ PASS |

### Mock Devices
- **STM32H743ZI MCU** — 3.3 V, 42.5 °C nominal, 115 mA
- **Xilinx Artix-7 FPGA** — 1.8 V, 58.0 °C nominal, 480 mA
- **FTDI FT2232H JTAG** — 5.0 V, 31.0 °C nominal, 45 mA

### Real Test Output
```
  [ RUN ] Phase 5 - discover: returns 3 default devices with correct IDs
  [ OK  ] Phase 5 - discover: returns 3 default devices with correct IDs
  [ RUN ] Phase 5 - connect/disconnect: state transitions are correct
  [ OK  ] Phase 5 - connect/disconnect: state transitions are correct
  [ RUN ] Phase 5 - telemetry: temperature and voltage fluctuate realistically
  [ OK  ] Phase 5 - telemetry: temperature and voltage fluctuate realistically
  [ RUN ] Phase 5 - fault injection: timeout sets fault_detected=true
  [ OK  ] Phase 5 - fault injection: timeout sets fault_detected=true
  [ RUN ] Phase 5 - recovery: recover clears fault and restores CONNECTED state
  [ OK  ] Phase 5 - recovery: recover clears fault and restores CONNECTED state
  [ RUN ] Phase 5 - reset: reinitialises all devices to defaults
  [ OK  ] Phase 5 - reset: reinitialises all devices to defaults
```

---

## Phase 6 — Packaging & Final Evidence  (2026-10-31)
**Status:** ✅ COMPLETE

### What was done
| Item | Status |
|------|--------|
| Clean rebuild from empty build/ | ✅ |
| All 61 tests passing, 0 warnings, -Werror clean | ✅ |
| README.md updated with accurate final status | ✅ |
| FINAL_SUBMISSION.md evidence document created | ✅ |
| PROGRESS.md completed for all phases | ✅ |
| Known limitations documented honestly | ✅ |

### Final Test Run
```
Result: 61 passed, 0 failed.
```

Compiler: g++ (MinGW-w64 GCC 13+) · Flags: -std=c++17 -Wall -Wextra -Wpedantic -Werror
Build: 18/18 objects · 0 errors · 0 warnings



