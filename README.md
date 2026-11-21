# MINI3

**Renderer-agnostic 3D engine.** The core knows nothing about any backend;
each backend implements a small vtable. Built to plug in: native (OpenGL /
Vulkan / WebGPU), browser (WebGL 2), or test doubles — without touching the
core.

- **Core:** math, scene graph, transforms, cameras, geometry, materials,
  lighting, frame builder — portable C11, no browser APIs, no renderer
  dependency.
- **One renderer interface** (`include/mini3/m3_renderer.h`): create/destroy,
  viewport, geometry/shader resources, begin/submit/end frame.
- **Flat + Gouraud shading only** — no shadows, PBR, post-processing, or
  physics. Deliberately lightweight.

## Quick start

```sh
# build & run the full native test suite
make test

# native demo: rotating, lit cube (software reference backend)
make demo
#   -> out/cube_frame_000.ppm, out/cube_frame_001.ppm, out/cube_frame_002.ppm
```

With CMake (optional):

```sh
cmake -B build -G "Unix Makefiles"
cmake --build build
ctest --test-dir build --output-on-failure
./build/cube_native 3 30 128 128 out
```

The WebGL 2 demo is static HTML/JS (see `docs/WEBGL2_DEMO.md`):

```sh
python3 -m http.server 8080 --directory demo/webgl2
# open http://localhost:8080/
```

Requirements: a C11 compiler and `make` (or CMake 3.20+). No required
dependencies beyond the C library. No renderer is required — the mock backend
plus the reference software rasterizer cover the full pipeline natively.

## Repository layout

```
include/mini3/       core public headers
src/                 core implementation (renderer-agnostic)
backends/mock/       mock backend (test-only, call-sequence recording)
backends/software/   CPU reference rasterizer (PPM export)
backends/webgl2/     JavaScript WebGL 2 backend (browser demo)
demo/webgl2/         static browser demo (rotating, lit cube + spheres)
examples/cube_native/  native end-to-end demo (software backend)
tests/               per-module native test suites
docs/                architecture + future-backend guide + demo docs
```

## Architecture

```
      Application
           |
   m3_renderer_interface (vtable)
           |
   +----------------------+
   |  Core (portable C)   |
   |  math, scene,        |
   |  geometry, light,    |
   |  camera, frame       |
   +----------+-----------+
              | vtable
   +----------+----------+
   |  Backends (no core   |
   |  header dependencies)|
   |  mock/software/webgl2|
   +----------------------+
```

See `docs/ARCHITECTURE.md` for the full picture, `docs/NATIVE_BACKENDS.md`
for the recipe to add a native backend, and `docs/WEBGL2_DEMO.md` for the
browser demo.

## Testing

- `make test` builds and runs all native suites: math, scene, geometry+material,
  camera, frame, mock backend, software rendering.
- `tests/test_software_render.c` exercises the full pipeline end-to-end
  (scene → frame → software backend → pixels) for flat and Gouraud shading.
- `tests/test_mock_backend.c` proves the renderer contract without GPUs.

## License

MIT — see `LICENSE`.

## Status

Active development. See `docs/ARCHITECTURE.md` for what is deliberately
excluded (no shadows, PBR, post, physics) and what to build next.
