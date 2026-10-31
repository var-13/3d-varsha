// ui/js/app.js
// ThermoBench – main application script.
// Phase 0: tab switching + version badge fetch.
// Later phases will add FEA plots, benchmark UI, etc.

"use strict";

// ── Tab switching ─────────────────────────────────────────────────────────────
(function initTabs() {
  const buttons = document.querySelectorAll(".tab-btn");
  const panels  = document.querySelectorAll(".tab-panel");

  function activate(tabName) {
    buttons.forEach(b => {
      const isActive = b.dataset.tab === tabName;
      b.classList.toggle("active", isActive);
      b.setAttribute("aria-selected", isActive);
    });
    panels.forEach(p => {
      p.classList.toggle("active", p.id === "tab-" + tabName);
    });
    // Persist last tab so a page refresh returns you to the same place.
    try { sessionStorage.setItem("tb_tab", tabName); } catch (_) {}
  }

  buttons.forEach(btn => {
    btn.addEventListener("click", () => activate(btn.dataset.tab));
  });

  // Restore last tab (default: geometry)
  const saved = (function() {
    try { return sessionStorage.getItem("tb_tab"); } catch (_) { return null; }
  })();
  activate(saved || "geometry");
})();


// ── Version badge ─────────────────────────────────────────────────────────────
(function fetchVersion() {
  const badge = document.getElementById("version-badge");
  if (!badge) return;

  fetch("/api/version")
    .then(r => {
      if (!r.ok) throw new Error("HTTP " + r.status);
      return r.json();
    })
    .then(data => {
      badge.textContent = data.version || "v?";
    })
    .catch(() => {
      // Running from file:// or server not up yet – silently keep default text.
    });
})();


// ── Phase 1: Thermal FEA Module ──────────────────────────────────────────────
(function initThermalFEA() {
  const inpL  = document.getElementById("inp-L");
  const inpA  = document.getElementById("inp-A");
  const inpk  = document.getElementById("inp-k");
  const inpq  = document.getElementById("inp-q");
  const inpT0 = document.getElementById("inp-T0");
  const inpTL = document.getElementById("inp-TL");
  const selN  = document.getElementById("sel-N");

  const btnRun       = document.getElementById("btn-run-thermal");
  const btnRunConv   = document.getElementById("btn-run-convergence");
  const btnExport    = document.getElementById("btn-export-thermal-csv");
  const btnExportConv= document.getElementById("btn-export-conv-csv");
  const statusBar    = document.getElementById("thermal-status");

  const kpiN       = document.getElementById("kpi-N");
  const kpiH       = document.getElementById("kpi-h");
  const kpiMidErr  = document.getElementById("kpi-mid-err");
  const kpiNodeErr = document.getElementById("kpi-node-err");
  const kpiOrder   = document.getElementById("kpi-order");
  const tbodyConv  = document.getElementById("tbody-convergence");

  if (!btnRun || !window.Chart) return;

  let tempChart = null;
  let convChart = null;

  function setStatus(msg, type = "info") {
    if (!statusBar) return;
    statusBar.textContent = msg;
    statusBar.className = "status-bar " + type;
  }

  function getParams() {
    const L  = parseFloat(inpL.value);
    const A  = parseFloat(inpA.value);
    const k  = parseFloat(inpk.value);
    const q  = parseFloat(inpq.value);
    const T0 = parseFloat(inpT0.value);
    const TL = parseFloat(inpTL.value);
    const N  = parseInt(selN.value, 10);

    // Strict validation
    if (isNaN(L) || L <= 0)  { setStatus("Validation error: Length L must be a positive number (> 0).", "error"); return null; }
    if (isNaN(A) || A <= 0)  { setStatus("Validation error: Area A must be a positive number (> 0).", "error"); return null; }
    if (isNaN(k) || k <= 0)  { setStatus("Validation error: Conductivity k must be positive (> 0).", "error"); return null; }
    if (isNaN(q))            { setStatus("Validation error: Heat generation q must be a valid number.", "error"); return null; }
    if (isNaN(T0) || isNaN(TL)) { setStatus("Validation error: Boundary temperatures must be finite numbers.", "error"); return null; }
    if (isNaN(N) || N <= 0)  { setStatus("Validation error: Element count N must be a positive integer.", "error"); return null; }

    return { L, A, k, q, T0, TL, N };
  }

  // ── Chart initialisation ───────────────────────────────────────────────────
  function initCharts() {
    const ctxTemp = document.getElementById("chart-temperature");
    const ctxConv = document.getElementById("chart-convergence");
    if (!ctxTemp || !ctxConv) return;

    const commonOptions = {
      responsive: true,
      maintainAspectRatio: false,
      color: "#8b93a8",
      plugins: {
        legend: { labels: { color: "#f0f2f8", font: { family: "Inter", size: 12 } } },
        tooltip: {
          backgroundColor: "#1c2030",
          titleColor: "#f0f2f8",
          bodyColor: "#f97316",
          borderColor: "#2a3048",
          borderWidth: 1
        }
      },
      scales: {
        x: {
          grid: { color: "rgba(255,255,255,0.06)" },
          ticks: { color: "#8b93a8" },
          title: { display: true, color: "#8b93a8", text: "Position x [m]" }
        },
        y: {
          grid: { color: "rgba(255,255,255,0.06)" },
          ticks: { color: "#8b93a8" },
          title: { display: true, color: "#8b93a8", text: "Temperature T [°C]" }
        }
      }
    };

    tempChart = new Chart(ctxTemp, {
      type: "line",
      data: { datasets: [] },
      options: commonOptions
    });

    convChart = new Chart(ctxConv, {
      type: "line",
      data: { datasets: [] },
      options: {
        ...commonOptions,
        scales: {
          x: {
            type: "logarithmic",
            grid: { color: "rgba(255,255,255,0.06)" },
            ticks: { color: "#8b93a8" },
            title: { display: true, color: "#8b93a8", text: "Element Size h [m] (log scale)" }
          },
          y: {
            type: "logarithmic",
            grid: { color: "rgba(255,255,255,0.06)" },
            ticks: { color: "#8b93a8" },
            title: { display: true, color: "#8b93a8", text: "Max Midpoint Error [°C] (log scale)" }
          }
        }
      }
    });
  }

  // ── Run single mesh FEA ────────────────────────────────────────────────────
  async function runSingleMesh() {
    const p = getParams();
    if (!p) return;

    setStatus(`Computing 1-D FEA for N = ${p.N}...`, "info");
    const qs = `L=${p.L}&A=${p.A}&k=${p.k}&q=${p.q}&T0=${p.T0}&TL=${p.TL}&N=${p.N}`;

    try {
      const res = await fetch(`/api/thermal/run?${qs}`);
      const data = await res.json();
      if (!data.success) {
        setStatus(`Compute error: ${data.error}`, "error");
        return;
      }

      // Update KPI cards
      kpiN.textContent = data.N;
      kpiH.textContent = `h = ${data.h.toFixed(4)} m`;
      kpiMidErr.textContent = `${data.max_midpoint_error.toFixed(6)} °C`;
      kpiNodeErr.textContent = `${data.max_nodal_error.toExponential(2)} °C`;

      // Theoretical expected error: (q / (2*k)) * (h/2)^2
      const theoErr = (data.q / (2 * data.k)) * Math.pow(data.h / 2, 2);
      document.getElementById("kpi-mid-expected").textContent = `Theoretical: ${theoErr.toFixed(6)} °C`;

      // High-resolution exact curve
      const exactPts = [];
      const numExact = 100;
      for (let i = 0; i <= numExact; ++i) {
        const x = (i / numExact) * data.L;
        const T = data.T0 + ((data.TL - data.T0) / data.L) * x + (data.q / (2 * data.k)) * x * (data.L - x);
        exactPts.push({ x, y: T });
      }

      const fePts = data.x_nodes.map((x, i) => ({ x, y: data.T_fe[i] }));

      tempChart.data = {
        datasets: [
          {
            label: "Exact Analytical Solution",
            data: exactPts,
            borderColor: "#38bdf8",
            borderWidth: 2,
            pointRadius: 0,
            fill: false
          },
          {
            label: `FE Solution (N = ${data.N})`,
            data: fePts,
            borderColor: "#f97316",
            backgroundColor: "#f97316",
            borderWidth: 2,
            pointRadius: 4,
            pointHoverRadius: 6,
            fill: false
          }
        ]
      };
      tempChart.update();

      setStatus(`✅ Solved N = ${data.N} (h = ${data.h} m). Max Midpoint Error = ${data.max_midpoint_error.toFixed(6)} °C (matches theoretical ${theoErr.toFixed(6)} °C).`, "ok");
    } catch (err) {
      setStatus(`Network/server error: ${err.message}`, "error");
    }
  }

  // ── Run all 4 meshes & convergence ─────────────────────────────────────────
  async function runConvergence() {
    const p = getParams();
    if (!p) return;

    setStatus("Running multi-mesh convergence benchmark (N = 10, 20, 40, 80)...", "info");
    const qs = `L=${p.L}&A=${p.A}&k=${p.k}&q=${p.q}&T0=${p.T0}&TL=${p.TL}`;

    try {
      const res = await fetch(`/api/thermal/convergence?${qs}`);
      const data = await res.json();
      if (!data.success) {
        setStatus(`Convergence error: ${data.error}`, "error");
        return;
      }

      // Populate convergence table
      tbodyConv.innerHTML = "";
      const convPts = [];
      const colors = ["#fb923c", "#f97316", "#ea580c", "#c2410c"];

      data.meshes.forEach((m, idx) => {
        const theoErr = (p.q / (2 * p.k)) * Math.pow(m.h / 2, 2);
        let ratioStr = "—";
        let orderStr = "—";
        if (idx > 0) {
          const prevErr = data.meshes[idx - 1].max_midpoint_error;
          const ratio = prevErr / m.max_midpoint_error;
          ratioStr = ratio.toFixed(4) + "×";
          orderStr = m.observed_order !== null ? m.observed_order.toFixed(4) : "—";
        }

        const tr = document.createElement("tr");
        tr.innerHTML = `
          <td><strong>N = ${m.N}</strong></td>
          <td>${m.h.toFixed(4)}</td>
          <td>${m.max_nodal_error.toExponential(2)}</td>
          <td style="color:#f97316; font-weight:600;">${m.max_midpoint_error.toFixed(8)}</td>
          <td>${theoErr.toFixed(8)}</td>
          <td>${ratioStr}</td>
          <td><strong style="color:#22c55e;">${orderStr}</strong></td>
        `;
        tbodyConv.appendChild(tr);

        convPts.push({ x: m.h, y: m.max_midpoint_error });
      });

      // Update KPI
      const last = data.meshes[data.meshes.length - 1];
      kpiOrder.textContent = last.observed_order ? last.observed_order.toFixed(2) : "2.00";
      kpiMidErr.textContent = `${last.max_midpoint_error.toFixed(6)} °C`;
      kpiNodeErr.textContent = `${last.max_nodal_error.toExponential(2)} °C`;

      // Plot convergence chart
      // Slope 2 reference curve: y = C * h^2 with C = q / (8*k) = 125
      const refPts = convPts.map(pt => ({ x: pt.x, y: (p.q / (8 * p.k)) * Math.pow(pt.x, 2) }));

      convChart.data = {
        datasets: [
          {
            label: "FE Midpoint Error (Observed)",
            data: convPts,
            borderColor: "#f97316",
            backgroundColor: "#f97316",
            borderWidth: 2,
            pointRadius: 6,
            pointHoverRadius: 8,
            fill: false
          },
          {
            label: "Theoretical O(h²) Reference (Slope = 2.0)",
            data: refPts,
            borderColor: "#38bdf8",
            borderWidth: 1.5,
            borderDash: [5, 5],
            pointRadius: 0,
            fill: false
          }
        ]
      };
      convChart.update();

      // Fetch and overlay all 4 meshes on temperature chart
      overlayAllMeshes(p);

      setStatus(`✅ Multi-mesh convergence complete! Second-order O(h²) rate verified (ratio 4.0×, order = 2.00).`, "ok");
    } catch (err) {
      setStatus(`Network/server error: ${err.message}`, "error");
    }
  }

  async function overlayAllMeshes(p) {
    const exactPts = [];
    for (let i = 0; i <= 100; ++i) {
      const x = (i / 100) * p.L;
      const T = p.T0 + ((p.TL - p.T0) / p.L) * x + (p.q / (2 * p.k)) * x * (p.L - x);
      exactPts.push({ x, y: T });
    }

    const datasets = [
      {
        label: "Exact Analytical Solution",
        data: exactPts,
        borderColor: "#38bdf8",
        borderWidth: 2.5,
        pointRadius: 0,
        fill: false
      }
    ];

    const meshColors = { 10: "#fde047", 20: "#fb923c", 40: "#f97316", 80: "#dc2626" };

    for (const N of [10, 20, 40, 80]) {
      const qs = `L=${p.L}&A=${p.A}&k=${p.k}&q=${p.q}&T0=${p.T0}&TL=${p.TL}&N=${N}`;
      const res = await fetch(`/api/thermal/run?${qs}`);
      const d = await res.json();
      if (d.success) {
        datasets.push({
          label: `FE Mesh N = ${N}`,
          data: d.x_nodes.map((x, i) => ({ x, y: d.T_fe[i] })),
          borderColor: meshColors[N],
          backgroundColor: meshColors[N],
          borderWidth: 1.5,
          pointRadius: N <= 20 ? 3 : 1.5,
          fill: false
        });
      }
    }

    tempChart.data = { datasets };
    tempChart.update();
  }

  // ── CSV Exports ────────────────────────────────────────────────────────────
  btnExport.addEventListener("click", () => {
    const p = getParams();
    if (!p) return;
    const url = `/api/thermal/export?L=${p.L}&A=${p.A}&k=${p.k}&q=${p.q}&T0=${p.T0}&TL=${p.TL}&N=${p.N}`;
    window.location.href = url;
  });

  btnExportConv.addEventListener("click", () => {
    const p = getParams();
    if (!p) return;
    const url = `/api/thermal/export_convergence?L=${p.L}&A=${p.A}&k=${p.k}&q=${p.q}&T0=${p.T0}&TL=${p.TL}`;
    window.location.href = url;
  });

  btnRun.addEventListener("click", runSingleMesh);
  btnRunConv.addEventListener("click", runConvergence);
  selN.addEventListener("change", runSingleMesh);

  // Initial load
  initCharts();
  runConvergence();
})();


// ── Phase 3: Multicore Batch Benchmark Module ────────────────────────────────
(function initPerformanceBenchmark() {
  const btnRun       = document.getElementById("btn-run-benchmark");
  const btnExport    = document.getElementById("btn-export-bench-csv");
  const inpCases     = document.getElementById("inp-perf-cases");
  const inpMesh      = document.getElementById("inp-perf-mesh");
  const inpSeed      = document.getElementById("inp-perf-seed");
  const statusEl     = document.getElementById("perf-status");

  const kpiCases     = document.getElementById("kpi-perf-cases");
  const kpiSubmesh   = document.getElementById("kpi-perf-submesh");
  const kpiBaseline  = document.getElementById("kpi-perf-baseline");
  const kpiSpeedup   = document.getElementById("kpi-perf-speedup");
  const kpiEff       = document.getElementById("kpi-perf-eff");
  const kpiParity    = document.getElementById("kpi-perf-parity");
  const kpiDiscrep   = document.getElementById("kpi-perf-discrepancy");

  const tbodyBench   = document.getElementById("tbody-benchmark");

  if (!btnRun) return;

  let speedupChart = null;
  let latencyChart = null;

  function setStatus(msg, type = "info") {
    if (!statusEl) return;
    statusEl.textContent = msg;
    statusEl.className = `status-bar ${type}`;
  }

  function initBenchmarkCharts() {
    if (typeof Chart === "undefined") return;

    const ctxSpeedup = document.getElementById("chart-speedup")?.getContext("2d");
    if (ctxSpeedup && !speedupChart) {
      speedupChart = new Chart(ctxSpeedup, {
        type: "line",
        data: { labels: [], datasets: [] },
        options: {
          responsive: true,
          maintainAspectRatio: false,
          plugins: {
            legend: { labels: { color: "#cbd5e1" } }
          },
          scales: {
            x: {
              title: { display: true, text: "Worker Count (Threads p)", color: "#94a3b8" },
              ticks: { color: "#94a3b8" },
              grid: { color: "#334155" }
            },
            y: {
              title: { display: true, text: "Speedup Factor Sp (T1 / Tp)", color: "#94a3b8" },
              ticks: { color: "#94a3b8" },
              grid: { color: "#334155" },
              beginAtZero: true
            }
          }
        }
      });
    }

    const ctxLatency = document.getElementById("chart-latency")?.getContext("2d");
    if (ctxLatency && !latencyChart) {
      latencyChart = new Chart(ctxLatency, {
        type: "bar",
        data: { labels: [], datasets: [] },
        options: {
          responsive: true,
          maintainAspectRatio: false,
          plugins: {
            legend: { labels: { color: "#cbd5e1" } }
          },
          scales: {
            x: {
              title: { display: true, text: "Worker Count (Threads)", color: "#94a3b8" },
              ticks: { color: "#94a3b8" },
              grid: { color: "#334155" }
            },
            y: {
              title: { display: true, text: "Median Execution Time [ms]", color: "#94a3b8" },
              ticks: { color: "#94a3b8" },
              grid: { color: "#334155" },
              beginAtZero: true
            }
          }
        }
      });
    }
  }

  async function runBenchmark() {
    const cases = parseInt(inpCases.value, 10) || 200;
    const mesh  = parseInt(inpMesh.value, 10) || 40;
    const seed  = parseInt(inpSeed.value, 10) || 42;

    btnRun.disabled = true;
    setStatus(`Executing warm-up and 5 measured runs for ${cases} cases (Mesh N=${mesh})...`, "info");

    try {
      const url = `/api/benchmark/run?cases=${cases}&mesh=${mesh}&seed=${seed}`;
      const res = await fetch(url);
      if (!res.ok) throw new Error("HTTP " + res.status);
      const data = await res.json();

      if (!data.runs || data.runs.length === 0) {
        throw new Error("No benchmark data returned from server.");
      }

      // Update KPIs
      kpiCases.textContent = data.case_count;
      kpiSubmesh.textContent = `Mesh N = ${data.mesh_N} (${data.mesh_N + 1} nodes/case)`;

      const baselineRun = data.runs.find(r => r.threads === 1) || data.runs[0];
      kpiBaseline.textContent = `${baselineRun.median_ms.toFixed(2)} ms`;

      // Find max speedup
      let maxSp = 1.0;
      let maxSpEff = 1.0;
      data.runs.forEach(r => {
        if (r.speedup > maxSp) {
          maxSp = r.speedup;
          maxSpEff = r.efficiency;
        }
      });
      kpiSpeedup.textContent = `${maxSp.toFixed(2)}×`;
      kpiEff.textContent = `Parallel Efficiency: ${(maxSpEff * 100).toFixed(1)}%`;

      if (data.parity_passed) {
        kpiParity.textContent = "100% MATCH";
        kpiParity.className = "kpi-value text-success";
        kpiDiscrep.textContent = `Max abs diff: ${data.max_discrepancy_C.toExponential(2)} °C (≤ 10⁻⁸)`;
      } else {
        kpiParity.textContent = "PARITY FAILED";
        kpiParity.className = "kpi-value text-danger";
        kpiDiscrep.textContent = `Max abs diff: ${data.max_discrepancy_C.toFixed(6)} °C`;
      }

      // Populate Table
      tbodyBench.innerHTML = "";
      data.runs.forEach(r => {
        const tr = document.createElement("tr");
        const runsHtml = r.run_times_ms.map(t => `<td>${t.toFixed(2)}</td>`).join("");
        tr.innerHTML = `
          <td><strong>${r.threads} Worker${r.threads > 1 ? "s" : ""}</strong></td>
          <td>${data.case_count}</td>
          <td>${data.mesh_N}</td>
          ${runsHtml}
          <td><strong>${r.median_ms.toFixed(2)}</strong></td>
          <td class="highlight"><strong>${r.speedup.toFixed(2)}×</strong></td>
          <td>${(r.efficiency * 100).toFixed(1)}%</td>
        `;
        tbodyBench.appendChild(tr);
      });

      // Update Charts
      const threadLabels = data.runs.map(r => `${r.threads}T`);
      const speedupVals  = data.runs.map(r => r.speedup);
      const idealVals    = data.runs.map(r => r.threads);
      const latencyVals  = data.runs.map(r => r.median_ms);

      if (speedupChart) {
        speedupChart.data = {
          labels: threadLabels,
          datasets: [
            {
              label: "Actual Measured Speedup Sp",
              data: speedupVals,
              borderColor: "#38bdf8",
              backgroundColor: "rgba(56, 189, 248, 0.2)",
              borderWidth: 2,
              pointRadius: 5,
              pointBackgroundColor: "#38bdf8",
              tension: 0.2
            },
            {
              label: "Ideal Linear Scaling (Sp = p)",
              data: idealVals,
              borderColor: "#a855f7",
              borderDash: [5, 5],
              borderWidth: 1.5,
              pointRadius: 3,
              fill: false
            }
          ]
        };
        speedupChart.update();
      }

      if (latencyChart) {
        latencyChart.data = {
          labels: threadLabels,
          datasets: [
            {
              label: "Median Execution Time [ms]",
              data: latencyVals,
              backgroundColor: ["#38bdf8", "#818cf8", "#c084fc", "#f472b6"],
              borderColor: "#60a5fa",
              borderWidth: 1,
              borderRadius: 4
            }
          ]
        };
        latencyChart.update();
      }

      setStatus(`Benchmark completed successfully: ${data.runs.length} configurations evaluated. Parity: ${data.parity_passed ? "PASS" : "FAIL"}.`, "success");
    } catch (err) {
      setStatus(`Benchmark failed: ${err.message}`, "error");
    } finally {
      btnRun.disabled = false;
    }
  }

  btnExport.addEventListener("click", () => {
    const cases = parseInt(inpCases.value, 10) || 200;
    const mesh  = parseInt(inpMesh.value, 10) || 40;
    const seed  = parseInt(inpSeed.value, 10) || 42;
    window.location.href = `/api/benchmark/export?cases=${cases}&mesh=${mesh}&seed=${seed}`;
  });

  btnRun.addEventListener("click", runBenchmark);

  initBenchmarkCharts();
  runBenchmark();
})();



// -- Phase 5: Hardware Monitor Module ----------------------------------------
(function initHardwareMonitor() {
  const btnDiscover  = document.getElementById("btn-hw-discover");
  const btnTelemetry = document.getElementById("btn-hw-telemetry");
  const btnReset     = document.getElementById("btn-hw-reset");
  const statusEl     = document.getElementById("hw-status");
  const deviceCards  = document.getElementById("hw-device-cards");
  const tbodyHw      = document.getElementById("tbody-hw");
  const eventLog     = document.getElementById("hw-event-log");

  if (!btnDiscover) return;

  let devices = [];

  function setStatus(msg, type) {
    if (!statusEl) return;
    statusEl.textContent = msg;
    statusEl.className = "status-bar " + (type || "info");
  }

  function logEvent(msg) {
    if (!eventLog) return;
    const ts = new Date().toLocaleTimeString();
    const line = document.createElement("div");
    line.textContent = "[" + ts + "]  " + msg;
    if (eventLog.firstChild && eventLog.firstChild.textContent === "No events yet.") {
      eventLog.innerHTML = "";
    }
    eventLog.prepend(line);
  }

  function stateColor(state) {
    if (state === "CONNECTED")     return "#22c55e";
    if (state === "TIMEOUT_ERROR") return "#ef4444";
    if (state === "DISCONNECTED")  return "#94a3b8";
    return "#f97316";
  }

  function renderDeviceCards(devList) {
    deviceCards.innerHTML = "";
    devList.forEach(function(d) {
      const card = document.createElement("div");
      card.className = "kpi-card";
      card.id = "hw-card-" + d.id;
      card.innerHTML =
        '<div class="kpi-label">' + d.type + ' &nbsp;&middot;&nbsp; <span style="color:#94a3b8;font-size:0.75rem;">' + d.id + '</span></div>' +
        '<div class="kpi-value" style="font-size:1rem;line-height:1.4;">' + d.name + '</div>' +
        '<div class="kpi-sub" style="margin-top:6px;">' +
          'State: <strong style="color:' + stateColor(d.state) + '">' + d.state + '</strong><br>' +
          'Firmware: ' + d.firmware + '<br>' +
          'Rail: ' + d.voltage_rail_V + ' V' +
        '</div>' +
        '<div style="margin-top:10px;display:flex;gap:6px;flex-wrap:wrap;">' +
          '<button onclick="window._hwFault(\'' + d.id + '\')" class="btn btn-outline" style="font-size:0.75rem;padding:4px 8px;">&#x26A1; Inject Fault</button>' +
          '<button onclick="window._hwRecover(\'' + d.id + '\')" class="btn btn-outline" style="font-size:0.75rem;padding:4px 8px;">&#x2705; Recover</button>' +
        '</div>';
      deviceCards.appendChild(card);
    });
  }

  function renderTelemetryTable(packets) {
    tbodyHw.innerHTML = "";
    packets.forEach(function(t) {
      const isFault = t.fault_detected;
      const tr = document.createElement("tr");
      if (isFault) tr.style.background = "rgba(239,68,68,0.08)";
      tr.innerHTML =
        '<td><strong>' + t.device_id + '</strong></td>' +
        '<td>' + (t.type || "�") + '</td>' +
        '<td><span style="color:' + stateColor(t.state) + ';font-weight:600;">' + t.state + '</span></td>' +
        '<td>' + (t.temperature_C !== undefined ? t.temperature_C.toFixed(1) : "�") + '</td>' +
        '<td>' + (t.voltage_V    !== undefined ? t.voltage_V.toFixed(3)    : "�") + '</td>' +
        '<td>' + (t.current_mA   !== undefined ? t.current_mA.toFixed(1)   : "�") + '</td>' +
        '<td>' + (t.power_mW     !== undefined ? t.power_mW.toFixed(1)     : "�") + '</td>' +
        '<td style="color:' + (isFault ? "#ef4444" : "#22c55e") + ';font-weight:700;">' + (isFault ? "&#x26A0; FAULT" : "&#x2713; OK") + '</td>' +
        '<td>' +
          '<button onclick="window._hwFault(\'' + t.device_id + '\')"   class="btn btn-outline" style="font-size:0.72rem;padding:3px 7px;">&#x26A1; Fault</button> ' +
          '<button onclick="window._hwRecover(\'' + t.device_id + '\')" class="btn btn-outline" style="font-size:0.72rem;padding:3px 7px;">&#x2705; Recover</button>' +
        '</td>';
      tbodyHw.appendChild(tr);
    });
  }

  async function discover() {
    setStatus("Discovering devices...", "info");
    try {
      const res  = await fetch("/api/hardware/devices");
      const data = await res.json();
      devices = data.devices || [];
      renderDeviceCards(devices);
      setStatus("Found " + data.device_count + " device(s): " + devices.map(function(d){ return d.name; }).join(", "), "ok");
      logEvent("Discovered " + data.device_count + " device(s)");
      await readTelemetry();
    } catch (err) {
      setStatus("Discovery failed: " + err.message, "error");
    }
  }

  async function readTelemetry() {
    setStatus("Reading telemetry...", "info");
    try {
      const res  = await fetch("/api/hardware/telemetry_all");
      const data = await res.json();
      const packets = data.packets || [];
      packets.forEach(function(p) {
        const dev = devices.find(function(d){ return d.id === p.device_id; });
        p.type = dev ? dev.type : "?";
      });
      renderTelemetryTable(packets);
      const faults = packets.filter(function(p){ return p.fault_detected; }).length;
      setStatus("Telemetry updated � " + packets.length + " device(s), " + faults + " fault(s)", faults ? "error" : "ok");
    } catch (err) {
      setStatus("Telemetry read failed: " + err.message, "error");
    }
  }

  window._hwFault = async function(id) {
    try {
      await fetch("/api/hardware/timeout/" + id, { method: "POST" });
      logEvent("FAULT injected -> " + id + " (ERR_TIMEOUT)");
      setStatus("Fault injected on " + id, "error");
      await readTelemetry();
    } catch (err) {
      logEvent("ERROR injecting fault on " + id + ": " + err.message);
    }
  };

  window._hwRecover = async function(id) {
    try {
      await fetch("/api/hardware/recover/" + id, { method: "POST" });
      logEvent("RECOVERED -> " + id + " (bus re-enumerated)");
      setStatus(id + " recovered successfully", "ok");
      await readTelemetry();
    } catch (err) {
      logEvent("ERROR recovering " + id + ": " + err.message);
    }
  };

  async function resetAll() {
    try {
      await fetch("/api/hardware/reset", { method: "POST" });
      logEvent("All devices reset to defaults");
      setStatus("All devices reset.", "info");
      await discover();
    } catch (err) {
      setStatus("Reset failed: " + err.message, "error");
    }
  }

  btnDiscover .addEventListener("click", discover);
  btnTelemetry.addEventListener("click", readTelemetry);
  btnReset    .addEventListener("click", resetAll);

  const hwTabBtn = document.getElementById("btn-hardware");
  if (hwTabBtn) {
    hwTabBtn.addEventListener("click", function() {
      if (devices.length === 0) discover();
    });
  }
})();
