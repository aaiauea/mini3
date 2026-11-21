# Guide: Writing a Future Native Backend

This guide is the contract checklist for adding a new real backend
(OpenGL / Vulkan / WebGPU / Metal / D3D12 / SDL+GPU). Keep it readable —
the mock backend (`backends/mock/m3_mock_backend.c`) and the software
reference rasterizer (`backends/software/m3_software.c`) are both complete
working examples, and the WebGL 2 browser backend
(`backends/webgl2/m3-webgl2-backend.js`) shows the contract as JS.

## 1. What the core guarantees (you can rely on these)

- `m3_frame` contains `view`, `proj`, `lights` (ambient + directional[4]),
  and `draws` (an ordered list of `m3_draw_item { mesh, material, model }`).
- Geometry resources are created once per `m3_mesh` (not per draw).
- Shaders are created once per shading mode (flat / Gouraud).
- `begin_frame` → `submit` (per draw item) → `end_frame` is the exact order.
- The core never includes any backend header. The backend never includes
  core headers beyond the public `mini3/m3_renderer.h` contract.

## 2. Implement the interface

Copy the `m3_renderer_interface` shape from `include/mini3/m3_renderer.h`
and fill every slot. The semantics are listed on that header; the rules:

| Function | Contract |
|---|---|
| `create(user, caps)` | Return an opaque backend pointer, or NULL on failure. Never `global.pthread`-style; no side effects beyond resources |
| `destroy(backend)` | Free everything; safe to call on a partially-created backend |
| `set_viewport(x,y,w,h)` | Clamp and store; `begin_frame` may rely on this |
| `create_geometry(mesh)` | Allocate a resource from `mesh`. MUST handle flat per-face normals and per-vertex normals |
| `create_shader(mode)` | Return NULL if `mode` is unsupported. Cache by mode internally |
| `begin_frame(frame)` | Set up the render pass: clear, depth test, viewport, pass `frame.view/proj` as uniforms (backend-private) |
| `submit(frame, draw, geometry, shader)` | Draw ONE draw item. Upload model matrix + normal matrix + material + first directional light |
| `end_frame(frame)` | Submit the frame (present, or for CPU backends: leave the buffer ready for `*_get_pixel`) |

Return codes: `create*` return NULL on failure; `set_viewport` may return
bool; `begin_frame` / `submit` / `end_frame` return void.

## 3. Pipeline shape

```
frame.view/proj ──matrix4 ──> viewProj = proj * view
for each draw: model (world matrix) ──> clip through proj * view * model
                │ (vertex shader)
                └── vertex position ──> rasterization
                └── normal matrix (transpose(inverse(upper3x3(model))))
                └── lighting: ambient + directional[0..N] · (color · (n·l))
                          per-vertex or per-fragment (flat vs Gouraud)
```

Depth: `proj` maps near→-1, far→+1 (GL conventions).

## 4. Reference backends (stepping stones)

- `backends/mock/m3_mock_backend.c` — zero-cost contract checker; used by
  the renderer-interface test. Start here to understand the contract.
- `backends/software/m3_software.c` — full CPU reference rasterizer:
  perspective-correct depth, flat + Gouraud, z-buffer, PPM export.
  Read this to understand what a correct backend actually does
  (barycentric interpolation, perspective correction, depth test).
- `backends/webgl2/m3-webgl2-backend.js` — the browser implementation,
  same contract, GLSL. Read it to see how the same shape maps to
  the GPU.

## 5. Checklist for the new backend

- [ ] Compiles against the core headers only
- [ ] Implements every vtable slot with the semantics above
- [ ] `create_geometry` handles meshes with re-tessellated normals
- [ ] `create_shader` handles BOTH flat and Gouraud (or returns NULL for
      one)
- [ ] No backend-specific code in the core (nothing in `src/` may reference
      this backend)
- [ ] A test in `tests/` exercising the backend end-to-end
- [ ] Documented usage in `docs/` mirroring the native cube demo
- [ ] README updated to list the new backend
