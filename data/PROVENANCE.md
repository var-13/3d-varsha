# Data Provenance Documentation

## 1. Overview
All geometry and simulation datasets provided in the `data/` directory of ThermoBench are synthetically generated for benchmarking, testing, and educational evaluation purposes. **No proprietary, confidential, or export-controlled CAD files or client simulation data are included.**

---

## 2. Geometry File (`data/sample.stl`)
- **Format:** Standard ASCII stereolithography (`.stl`).
- **Entity:** Extruded aluminium cold plate / heatsink base block ($50\text{ mm} \times 50\text{ mm} \times 10\text{ mm}$).
- **Coordinate System:** Right-handed Cartesian coordinates:
  - $x \in [0.000, 0.050]\text{ m}$ (width)
  - $y \in [0.000, 0.050]\text{ m}$ (length)
  - $z \in [0.000, 0.010]\text{ m}$ (height)
- **Topology:** 12 triangular facets, 6 planar faces, closed manifold surface.
- **Surface Area:** $0.0070\text{ m}^2$ ($7000\text{ mm}^2$).
- **Verification:** Parsed and validated by `thermobench::geometry::validate_stl` (checks facet syntax, surface normals, bounding box bounds, and finite numerical values).

---

## 3. FEA Point Cloud File (`data/sample_ansys.csv`)
- **Format:** Comma-separated values (CSV).
- **Schema:** `node_id,x_m,y_m,z_m,temperature_C`
  - `node_id`: Positive integer index $[1, 65]$.
  - `x_m, y_m, z_m`: Node coordinates in metres ($[0.000, 0.050]\text{ m}$).
  - `temperature_C`: Nodal steady-state temperature in degrees Celsius ($[28.2^\circ\text{C}, 94.8^\circ\text{C}]$).
- **Physical Model:** Synthetic steady-state Gaussian hotspot thermal profile generated via:
  $$T(x, y, z) = T_{\infty} + \Delta T_{\max} \cdot \exp\left(-\frac{(x - x_c)^2 + (y - y_c)^2}{2\sigma^2}\right) \cdot \left(1 - \frac{z}{2h}\right)$$
  where $x_c = y_c = 0.025\text{ m}$, $T_\infty = 25.0^\circ\text{C}$, $\Delta T_{\max} = 70.0^\circ\text{C}$, $\sigma = 0.012\text{ m}$, and $h = 0.010\text{ m}$.
- **Verification:** Verified by `thermobench::geometry::validate_pointcloud` ensuring all coordinates match the geometry bounds, no NaN/Inf values exist, and thermal bounds are physically consistent.
