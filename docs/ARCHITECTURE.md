# MINI3 Architecture

MINI3 is a renderer-agnostic 3D engine. The core knows nothing about any
backend — it only knows the `m3_renderer_interface` vtable. Each backend
(native software rasterizer, (mock test backend), WebGL 2 browser, and later
OpenGL/Vulkan/WebGPU) implements that one contract.

## Layer diagram

```
┌────────────────────────────────────────────────────┐
│  Application (examples, demos, tests, future games) │
└──────────────┬─────────────────────────────────────┘
               │ m3_renderer_interface (vtable)
┌──────────────▼─────────────────────────────────────┐
│  Core (renderer-agnostic, portable C)               │
│                                                      │
│  math ── scene graph ── geometry ── material ──     │
│  lighting ── camera ── frame ── renderer contract    │
└──────────────┬─────────────────────────────────────┘
               │ vtable (create/destroy/setViewport/…)
┌──────────────▼─────────────────────────────────────┐
│  Backends (never linked into the core)              │
│  - mock    (unit tests)                             │
│  - software (CPU reference rasterizer, PPM out)      │
│  - webgl2  (browser)                                 │
│  - future: OpenGL / Vulkan / WebGPU                   │
└────────────────────────────────────────────────────┘
```

## Core modules (`include/mini3/`, `src/`)

| Module | Header / Source | What it does |
|---|---|---|
| Math | `m3_math.h/.c` | `m3_vec3`, `m3_quat`, `m3_mat4` (column-major), compose/inverse/perspective/look_at, normal matrices |
| Scene graph | `m3_scene.h/.c` | Flat node list, parent/child links, live world-matrix walks, `m3_scene_walk`, drawable set |
| Geometry | `m3_geometry.h/.c` | `m3_mesh` (positions/indices/normals/colors), `m3_mesh_cube`, `m3_mesh_sphere`, `m3_mesh_plane` |
| Material | `m3_material.h/.c` | Diffuse color + `M3_SHADING_FLAT` / `M3_SHADING_GOURAUD` |
| Lighting | `m3_light.h/.c` | Ambient + up to 4 directional lights |
| Camera | `m3_camera.h/.c` | Perspective camera; `m3_camera_view`, `m3_camera_proj`, `m3_camera_view_proj`, `m3_camera_look_at` |
| Renderer contract | `m3_renderer.h/.c` | Capabilities + vtable + no-op wrappers — the ONLY core↔backend coupling |
| Frame | `m3_frame.h/.c` | `m3_frame` (view/proj/lights/draws) built by `m3_frame_build` |

## Ownership rules

- `m3_frame` owns nothing; it is a plain struct returned by `m3_frame_build`
  (and `m3_frame_free` reclaims the draw list).
- Resources created by `m3_renderer_interface::create_geometry` /
  `create_shader` are caller-owned; destroy with the matching `destroy_*`.
- The core never includes a backend header. The core NEVER branches on a
  backend's capabilities; it reports the requested shading mode via the
  `create_shader` argument, and a backend that can't honor it returns NULL.

## The one vtable

`include/mini3/m3_renderer.h` defines `m3_renderer_interface`:

```
create / destroy
set_viewport
create_geometry / destroy_geometry
create_shader  / destroy_shader
begin_frame / submit / end_frame
```

Everything else — a backend's internal data layout, shader caching, program
compilation — is backend-private. See `docs/NATIVE_BACKENDS.md` for the
recipe for adding a future real backend.

## Deliberately excluded (non-goals)

- Shadows, PBR, post-processing, physics, audio, animation
- Backend-specific code in the core (not allowed)
- An opinionated scene-graph format, engine-wide singletons, threading

## Testing model

- Core unit tests (`tests/test_math.c`, `test_scene.c`, …) run without any backend.
- Renderer-interface test (`tests/test_mock_backend.c`) drives the core
  through the mock backend with a synthetic frame and asserts the call
  sequence — this proves the contract without needing GPUs.
- Real backend test (`tests/test_software_render.c`) renders a rotating, lit
  cube via the software reference backend end-to-end and checks pixels.
