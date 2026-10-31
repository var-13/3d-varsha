# ThermoBench

> Windows 11 x64 engineering workbench — thermal FEA, backpropagation, multicore benchmarking, geometry parsing, and hardware simulation.  
> **Internship project. All data is synthetic. No credentials stored.**

---

## Quick Start (two commands — no PATH setup needed)

Open a plain PowerShell in the project root and run:

```powershell
.\build.ps1      # configure, compile, and run all 61 tests
.\start.ps1      # start the HTTP server
```

Then open **http://localhost:8080** in your browser.

> The scripts hard-code `C:\msys64\mingw64\bin` so `cmake`/`ninja`/`g++`
> are found automatically — no PATH changes required.

---

## Prerequisites

| Tool | Version | Where to get |
|------|---------|--------------|
| MSYS2 / MinGW-w64 | GCC ≥ 13 | https://www.msys2.org |
| CMake | ≥ 3.20 | bundled with MSYS2 |
| Ninja | any | bundled with MSYS2 |

Add `C:\msys64\mingw64\bin` to your user `PATH`.

---

## Architecture

```
ThermoBench
├── src/core/        Pure C++17 numerics (no UI/network code)
│   ├── fem1d        1-D FEM heat-conduction solver (Thomas algorithm)
│   ├── backprop     Backpropagation + gradient-descent trainer
│   ├── batch_solver Multicore parallel batch solver (std::thread)
│   ├── geometry     ASCII STL + ANSYS CSV point-cloud parsers
│   └── embedded_mock  MCU / FPGA / JTAG mock devices + telemetry
├── src/server/      Thin HTTP server (cpp-httplib). Routes only – no maths.
├── src/launcher/    Launch.exe – starts server, opens browser, handles shutdown.
├── ui/              Plain HTML/CSS/JS (one page, 4 tabs). No build step needed.
│   └── vendor/      Chart.js 4.4.3, Three.js r167 – saved locally, no CDN.
├── tests/           Tiny test runner + per-phase test files (56 tests, 0 deps).
├── data/            Synthetic sample STL, ANSYS CSV, PROVENANCE.md.
├── results/         Generated CSVs (git-ignored).
└── docs/            EMBEDDED.md, AI_LOG.md, PROGRESS.md, DEPENDENCIES.md.
```

---

## What Each Tab Does

| Tab | Feature | Core module |
|-----|---------|-------------|
| **Geometry** | Load ASCII STL or ANSYS CSV, view 3-D mesh (Three.js) | `geometry.cpp` |
| **Thermal Analysis** | 1-D FEM solver — set BCs, solve, plot T(x) | `fem1d.cpp` |
| **Performance** | Multicore batch solve + backprop training, live charts | `batch_solver.cpp`, `backprop.cpp` |
| **Hardware** | Discover MCU/FPGA/JTAG mocks, stream telemetry, inject faults | `embedded_mock.cpp` |

---

## Supported File Formats

| File type | Tab | Description |
|-----------|-----|-------------|
| ASCII STL | Geometry | Triangle mesh — validates structure, rejects NaN/Inf |
| CSV (nodes) | Geometry | Columns: node_id, x_m, y_m, z_m, temperature_C |

---

## Test Results

```
56 / 56 tests PASSED
  Phase 0 — Launcher / Build: 3 tests
  Phase 1 — FEM 1D Solver   : 12 tests
  Phase 2 — Backpropagation : 15 tests
  Phase 3 — Batch Solver    : 12 tests
  Phase 4 — Geometry Parsers: 8 tests
  Phase 5 — Embedded Mocks  : 6 tests
```

---

## Known Limitations

- Binary STL not supported (ASCII only).
- HTTP server uses plain HTTP; no TLS/HTTPS.
- Telemetry is simulated; no real hardware bus.
- Neural network is a small demo net (1 hidden layer, 3 inputs → 1 output).

---

## Licence

All original code: MIT.  
Third-party libraries: see [docs/DEPENDENCIES.md](docs/DEPENDENCIES.md).
