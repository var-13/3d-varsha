# ThermoBench – Dependency Pinning & Licences

All versions pinned as of 2026-10-31. No CDN use at runtime.

| Library | Version | Licence | Location in repo | Purpose |
|---------|---------|---------|-----------------|---------|
| [cpp-httplib](https://github.com/yhirose/cpp-httplib) | v0.16.3 | MIT | `vendor/httplib.h` | Header-only HTTP/1.1 server |
| [Three.js](https://threejs.org) | r167 | MIT | `ui/vendor/three.min.js` | 3-D STL viewer (Phase 4) |
| [Chart.js](https://www.chartjs.org) | 4.4.3 | MIT | `ui/vendor/chart.umd.min.js` | 2-D plots (Phases 1–3) |

## C++ Standard Library
- C++17, provided by MinGW-w64 (GCC 16.2.0) — GPLv3 with GCC Runtime Exception.

## Build tools (not bundled, user installs)
| Tool | Version | Licence |
|------|---------|---------|
| GCC/MinGW-w64 | 16.2.0 | GPLv3 + Runtime Exception |
| CMake | 4.4.5 | BSD-3-Clause |
| Ninja | 1.13.2 | Apache-2.0 |

## Notes
- All synthetic data (`data/sample.stl`, `data/sample_nodes.csv`) is original and has no licence restrictions.
- No API keys, credentials or personal data are stored anywhere in this repo.
