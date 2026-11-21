# WebGL 2 Browser Demo

`demo/webgl2/index.html` is a static page that renders a rotating, lit cube
plus orbiting spheres through the same renderer contract as the native
backends (mock / software). No build step — the page is served as-is.

## Quick start

```sh
# Serve from the repository root (the page imports the backend JS via a
# relative path, so repo-root serving keeps the module graph intact):
python3 -m http.server 8080
# then open:
#   http://localhost:8080/demo/webgl2/
```

Or open `demo/webgl2/index.html` directly in a browser (file:// URLs often
block module imports — serve it instead if so).

Requirements: a modern browser with WebGL 2 (Chrome, Edge, Firefox, Safari
16.4+). No build tools, no native extensions.

## What it shows

- Rotating, lit cube (flat + Gouraud shading)
- Orbiting spheres (same pipeline, per-draw model matrices)
- A fully self-contained implementation of the renderer contract in JS
  (`backends/webgl2/m3-webgl2-backend.js`)
- Keyboard shortcuts: F = flat, G = Gouraud, R = reset, space = auto-rotate

## Verification

The demo is **static HTML/JS, not executed in CI**. Verify it in a browser:

1. Load the page — a cube should appear, slowly rotating.
2. Press **G** (Gouraud) — the cube should look similar but with smooth
   per-vertex lighting; **F** returns to flat.
3. Move the Pitch slider — the camera should orbit up/down (turntable view).
4. Move the Balls slider — the number of orbiting spheres increases.
5. Toggle auto-rotate — the yaw should stop.

Notes:
- Pitch orbits the camera eye around the cube; yaw rotates the cube. They
  are independent, and auto-rotate animates yaw only.
- The demo intentionally mirrors `m3_frame_build` semantics but does NOT
  share C code (the browser backend is a separate implementation, not wasm).

## Native equivalent

`examples/cube_native/` renders the same scene via the software reference
backend and writes PPM frames:

```sh
make demo          # writes out/cube_frame_*.ppm (3 frames, 128x128)
./cube_native 10 22.5 256 256 out   # custom frames/rotation/sizes
```

The WebGL 2 demo is the browser-side mirror of the same pipeline. See
`docs/ARCHITECTURE.md` for how the two relate.
