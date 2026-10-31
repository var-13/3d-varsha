// ui/js/viewer3d.js
// Three.js 3D CAD and ANSYS Point Cloud Viewport for ThermoBench.
// Uses local vendor/three.module.min.js (zero CDN, 100% offline).

import * as THREE from '../vendor/three.module.min.js';

(function init3DViewer() {
  const container = document.getElementById("viewport-3d");
  if (!container) return;

  const btnToggleMesh  = document.getElementById("btn-toggle-mesh");
  const btnToggleNodes = document.getElementById("btn-toggle-nodes");
  const btnResetView   = document.getElementById("btn-reset-view");
  const btnToggleSpin  = document.getElementById("btn-toggle-spin");
  const statusEl       = document.getElementById("geo-status");

  const inspPlaceholder = document.getElementById("inspector-placeholder");
  const inspDetails     = document.getElementById("inspector-details");
  const inspId          = document.getElementById("insp-id");
  const inspTemp        = document.getElementById("insp-temp");
  const inspPos         = document.getElementById("insp-pos");
  const inspBadge       = document.getElementById("insp-badge");

  // Scene setup
  const scene = new THREE.Scene();
  scene.background = new THREE.Color(0x0a0d14);

  const width = container.clientWidth || 600;
  const height = container.clientHeight || 520;

  const camera = new THREE.PerspectiveCamera(45, width / height, 0.001, 10.0);
  camera.position.set(0.08, 0.06, 0.08);

  const renderer = new THREE.WebGLRenderer({ antialias: true });
  renderer.setSize(width, height);
  renderer.setPixelRatio(Math.min(window.devicePixelRatio, 2));
  renderer.toneMapping = THREE.ACESFilmicToneMapping;
  container.appendChild(renderer.domElement);

  // Lighting
  const ambLight = new THREE.AmbientLight(0xffffff, 0.7);
  scene.add(ambLight);

  const dirLight1 = new THREE.DirectionalLight(0xffedd5, 1.2);
  dirLight1.position.set(0.2, 0.3, 0.2);
  scene.add(dirLight1);

  const dirLight2 = new THREE.DirectionalLight(0x38bdf8, 0.6);
  dirLight2.position.set(-0.2, -0.1, -0.2);
  scene.add(dirLight2);

  // Grid
  const gridHelper = new THREE.GridHelper(0.12, 12, 0xf97316, 0x1e293b);
  gridHelper.position.y = -0.001;
  scene.add(gridHelper);

  // Groups
  const modelGroup = new THREE.Group();
  scene.add(modelGroup);

  let meshSolid = null;
  let meshWire  = null;
  let pointsObj = null;
  let nodeData  = [];
  let isSpinning = true;
  let meshMode = 0; // 0: solid+wire, 1: solid, 2: wire, 3: hidden

  // Colormap function: T in [minT, maxT] -> THREE.Color
  function getThermalColor(t, minT, maxT) {
    const norm = Math.max(0, Math.min(1, (t - minT) / (maxT - minT || 1)));
    // Hue from 240 (blue) down to 0 (red)
    const hue = (1.0 - norm) * 240.0;
    const color = new THREE.Color();
    color.setHSL(hue / 360.0, 0.95, 0.52);
    return color;
  }

  // Load STL Geometry
  async function loadGeometry() {
    try {
      const res = await fetch("/api/geometry/stl");
      if (!res.ok) throw new Error("HTTP " + res.status);
      const data = await res.json();

      const geom = new THREE.BufferGeometry();
      const positions = [];
      const normals = [];

      for (const tri of data.triangles) {
        // tri: [nx, ny, nz, v1x, v1y, v1z, v2x, v2y, v2z, v3x, v3y, v3z]
        const nx = tri[0], ny = tri[1], nz = tri[2];
        for (let v = 0; v < 3; ++v) {
          positions.push(tri[3 + v * 3], tri[4 + v * 3], tri[5 + v * 3]);
          normals.push(nx, ny, nz);
        }
      }

      geom.setAttribute('position', new THREE.Float32BufferAttribute(positions, 3));
      geom.setAttribute('normal', new THREE.Float32BufferAttribute(normals, 3));

      // Center geometry around origin
      geom.computeBoundingBox();
      const center = new THREE.Vector3();
      geom.boundingBox.getCenter(center);
      geom.translate(-center.x, -geom.boundingBox.min.y, -center.z);

      const matSolid = new THREE.MeshStandardMaterial({
        color: 0x334155,
        roughness: 0.35,
        metalness: 0.8,
        transparent: true,
        opacity: 0.88,
        side: THREE.DoubleSide
      });

      const matWire = new THREE.MeshBasicMaterial({
        color: 0xf97316,
        wireframe: true,
        transparent: true,
        opacity: 0.4
      });

      meshSolid = new THREE.Mesh(geom, matSolid);
      meshWire  = new THREE.Mesh(geom, matWire);

      modelGroup.add(meshSolid);
      modelGroup.add(meshWire);

      // Now load ANSYS FEA point cloud
      await loadPointCloud(center, geom.boundingBox.min.y);

      if (statusEl) {
        statusEl.textContent = `Loaded ${data.name} (${data.triangle_count} facets) & ${nodeData.length} FEA thermal nodes. Orbit with mouse.`;
        statusEl.className = "status-bar success";
      }
    } catch (err) {
      if (statusEl) {
        statusEl.textContent = `Error loading CAD/FEA data: ${err.message}`;
        statusEl.className = "status-bar error";
      }
    }
  }

  // Load ANSYS point cloud
  async function loadPointCloud(geomCenter, geomMinY) {
    const res = await fetch("/api/geometry/pointcloud");
    if (!res.ok) throw new Error("HTTP " + res.status);
    const data = await res.json();

    nodeData = data.nodes;
    const positions = [];
    const colors = [];

    const geom = new THREE.BufferGeometry();

    for (const node of data.nodes) {
      // node: [id, x, y, z, temp]
      const nx = node[1] - geomCenter.x;
      const ny = node[3] - geomMinY; // swap/align y & z according to standard CAD orientation
      const nz = node[2] - geomCenter.z;

      positions.push(nx, ny, nz);

      const col = getThermalColor(node[4], data.min_temperature_C, data.max_temperature_C);
      colors.push(col.r, col.g, col.b);
    }

    geom.setAttribute('position', new THREE.Float32BufferAttribute(positions, 3));
    geom.setAttribute('color', new THREE.Float32BufferAttribute(colors, 3));

    // Custom circular points
    const pMat = new THREE.PointsMaterial({
      size: 0.0035,
      vertexColors: true,
      transparent: true,
      opacity: 0.95
    });

    pointsObj = new THREE.Points(geom, pMat);
    modelGroup.add(pointsObj);
  }

  // Mouse Orbit / Pan / Zoom controller
  let isDragging = false;
  let dragButton = 0;
  let prevMouseX = 0;
  let prevMouseY = 0;
  let theta = Math.PI / 4;
  let phi   = Math.PI / 4;
  let radius = 0.12;
  const target = new THREE.Vector3(0, 0.005, 0);

  function updateCamera() {
    phi = Math.max(0.05, Math.min(Math.PI / 2 - 0.05, phi));
    camera.position.x = target.x + radius * Math.sin(phi) * Math.sin(theta);
    camera.position.y = target.y + radius * Math.cos(phi);
    camera.position.z = target.z + radius * Math.sin(phi) * Math.cos(theta);
    camera.lookAt(target);
  }
  updateCamera();

  const canvas = renderer.domElement;

  canvas.addEventListener("mousedown", (e) => {
    isDragging = true;
    dragButton = e.button;
    prevMouseX = e.clientX;
    prevMouseY = e.clientY;
    isSpinning = false;
  });

  window.addEventListener("mouseup", () => {
    isDragging = false;
  });

  canvas.addEventListener("contextmenu", (e) => e.preventDefault());

  canvas.addEventListener("mousemove", (e) => {
    if (isDragging) {
      const dx = e.clientX - prevMouseX;
      const dy = e.clientY - prevMouseY;
      prevMouseX = e.clientX;
      prevMouseY = e.clientY;

      if (dragButton === 0) {
        // Orbit
        theta -= dx * 0.01;
        phi   -= dy * 0.01;
        updateCamera();
      } else if (dragButton === 2) {
        // Pan
        target.y += dy * 0.0002;
        updateCamera();
      }
    } else {
      // Raycasting for node hover
      handleRaycast(e);
    }
  });

  canvas.addEventListener("wheel", (e) => {
    e.preventDefault();
    radius = Math.max(0.02, Math.min(0.5, radius + e.deltaY * 0.0001));
    updateCamera();
  }, { passive: false });

  // Raycaster for node inspection
  const raycaster = new THREE.Raycaster();
  raycaster.params.Points.threshold = 0.002;
  const mouse = new THREE.Vector2();

  function handleRaycast(e) {
    if (!pointsObj || nodeData.length === 0) return;

    const rect = canvas.getBoundingClientRect();
    mouse.x = ((e.clientX - rect.left) / rect.width) * 2 - 1;
    mouse.y = -((e.clientY - rect.top) / rect.height) * 2 + 1;

    raycaster.setFromCamera(mouse, camera);
    const intersects = raycaster.intersectObject(pointsObj);

    if (intersects.length > 0) {
      const idx = intersects[0].index;
      if (idx !== undefined && idx < nodeData.length) {
        const node = nodeData[idx];
        showNodeInspector(node);
      }
    }
  }

  function showNodeInspector(node) {
    if (!inspDetails || !inspPlaceholder) return;
    inspPlaceholder.style.display = "none";
    inspDetails.style.display = "block";

    inspId.textContent = `#${node[0]}`;
    inspTemp.textContent = `${node[4].toFixed(2)} °C`;
    inspPos.textContent = `(${(node[1] * 1000).toFixed(1)}, ${(node[2] * 1000).toFixed(1)}, ${(node[3] * 1000).toFixed(1)}) mm`;

    if (node[4] > 80.0) {
      inspBadge.textContent = "CRITICAL HOTSPOT";
      inspBadge.className = "badge";
      inspBadge.style.color = "#f87171";
      inspBadge.style.borderColor = "rgba(239, 68, 68, 0.4)";
    } else if (node[4] > 55.0) {
      inspBadge.textContent = "MODERATE GRADIENT";
      inspBadge.className = "badge";
      inspBadge.style.color = "#facc15";
      inspBadge.style.borderColor = "rgba(234, 179, 8, 0.4)";
    } else {
      inspBadge.textContent = "COLD BOUNDARY";
      inspBadge.className = "badge";
      inspBadge.style.color = "#38bdf8";
      inspBadge.style.borderColor = "rgba(56, 189, 248, 0.4)";
    }
  }

  // Button actions
  if (btnToggleMesh) {
    btnToggleMesh.addEventListener("click", () => {
      meshMode = (meshMode + 1) % 4;
      if (meshMode === 0) {
        if (meshSolid) meshSolid.visible = true;
        if (meshWire) meshWire.visible = true;
        btnToggleMesh.textContent = "STL: Solid + Wireframe";
      } else if (meshMode === 1) {
        if (meshSolid) meshSolid.visible = true;
        if (meshWire) meshWire.visible = false;
        btnToggleMesh.textContent = "STL: Solid Only";
      } else if (meshMode === 2) {
        if (meshSolid) meshSolid.visible = false;
        if (meshWire) meshWire.visible = true;
        btnToggleMesh.textContent = "STL: Wireframe Only";
      } else {
        if (meshSolid) meshSolid.visible = false;
        if (meshWire) meshWire.visible = false;
        btnToggleMesh.textContent = "STL: Hidden";
      }
    });
  }

  if (btnToggleNodes) {
    btnToggleNodes.addEventListener("click", () => {
      if (pointsObj) {
        pointsObj.visible = !pointsObj.visible;
        btnToggleNodes.textContent = pointsObj.visible ? "Hide Point Cloud" : "Show Point Cloud";
      }
    });
  }

  if (btnResetView) {
    btnResetView.addEventListener("click", () => {
      theta = Math.PI / 4;
      phi   = Math.PI / 4;
      radius = 0.12;
      target.set(0, 0.005, 0);
      updateCamera();
    });
  }

  if (btnToggleSpin) {
    btnToggleSpin.addEventListener("click", () => {
      isSpinning = !isSpinning;
      btnToggleSpin.textContent = isSpinning ? "⏸ Pause Rotation" : "▶ Auto-Rotate";
    });
  }

  // Handle Resize
  window.addEventListener("resize", () => {
    const w = container.clientWidth || 600;
    const h = container.clientHeight || 520;
    camera.aspect = w / h;
    camera.updateProjectionMatrix();
    renderer.setSize(w, h);
  });

  // Animation Loop
  function animate() {
    requestAnimationFrame(animate);
    if (isSpinning) {
      modelGroup.rotation.y += 0.005;
    }
    renderer.render(scene, camera);
  }
  animate();

  loadGeometry();
})();
