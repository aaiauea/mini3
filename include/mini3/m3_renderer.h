/*
 * m3_renderer.h - Mini3 renderer contract (backend interface)
 *
 * This file is the ONLY coupling between the core and any backend. It is
 * deliberately a small vtable: lifecycle, viewport, resource management, and
 * frame boundaries. The CORE NEVER includes any backend implementation.
 *
 * Ownership & lifetime rules
 * --------------------------
 *   - A backend is created by calling `create` exactly once per backend
 *     instance and is torn down with its `destroy`.
 *   - Resources (geometry, shaders) returned by `create_geometry` /
 *     `create_shader` must be kept alive by the caller while they may be
 *     used in a frame and destroyed with the matching `destroy_*`.
 *   - Geometry is created once per `m3_mesh` (it is not per-draw). The same
 *     geometry resource can be submitted in many frames.
 *   - Shaders are created once per shading mode (flat/gouraud) and reused.
 *   - `begin_frame` / `submit` / `end_frame` must be called in that order,
 *     one frame at a time.
 *
 * Capabilities
 * ------------
 * `create` receives a capability struct. Backends may inspect it to decide
 * whether they can honor the requested features; the core never branches on
 * it. Backends that do not implement a mode return NULL from `create_shader`
 * (the caller is expected to reject it at setup).
 */
#ifndef M3_RENDERER_H
#define M3_RENDERER_H

#include "mini3/m3_geometry.h"
#include "mini3/m3_material.h"
#include "mini3/m3_light.h"
#include "mini3/m3_frame.h"
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    /* requested shading modes; backends report support via create_shader(NULL) */
    uint32_t flags;          /* future flags; 0 for now */
    int      max_lights;     /* directional lights it will honor; >= 1 */
} m3_renderer_caps;

/* The backend's user-provided config arrives via void* user pointer. */
typedef void *(*m3_renderer_create_fn)(void *user, const m3_renderer_caps *caps);
typedef void  (*m3_renderer_destroy_fn)(void *backend);

typedef bool  (*m3_renderer_set_viewport_fn)(void *backend, int32_t x, int32_t y,
                                            int32_t w, int32_t h);

typedef void *(*m3_renderer_create_geometry_fn)(void *backend, const m3_mesh *mesh);
typedef void  (*m3_renderer_destroy_geometry_fn)(void *backend, void *geometry);

typedef void *(*m3_renderer_create_shader_fn)(void *backend, m3_shading_mode mode);
typedef void  (*m3_renderer_destroy_shader_fn)(void *backend, void *shader);

typedef void  (*m3_renderer_begin_frame_fn)(void *backend, const m3_frame *frame);
typedef void  (*m3_renderer_submit_fn)(void *backend, const m3_frame *frame,
                                      const m3_draw_item *draw,
                                      void *geometry, void *shader);
typedef void  (*m3_renderer_end_frame_fn)(void *backend, const m3_frame *frame);

typedef struct {
    m3_renderer_create_fn          create;
    m3_renderer_destroy_fn         destroy;
    m3_renderer_set_viewport_fn    set_viewport;
    m3_renderer_create_geometry_fn create_geometry;
    m3_renderer_destroy_geometry_fn destroy_geometry;
    m3_renderer_create_shader_fn   create_shader;
    m3_renderer_destroy_shader_fn  destroy_shader;
    m3_renderer_begin_frame_fn     begin_frame;
    m3_renderer_submit_fn          submit;
    m3_renderer_end_frame_fn       end_frame;
} m3_renderer_interface;

/* Convenience helpers — runtime code calls the function pointers directly. */
void *m3_renderer_create(const m3_renderer_interface *renderer, void *user,
                         const m3_renderer_caps *caps);
void  m3_renderer_destroy(const m3_renderer_interface *renderer, void *backend);
bool  m3_renderer_set_viewport(const m3_renderer_interface *renderer, void *backend,
                               int32_t x, int32_t y, int32_t w, int32_t h);
void *m3_renderer_create_geometry(const m3_renderer_interface *renderer, void *backend,
                                  const m3_mesh *mesh);
void  m3_renderer_destroy_geometry(const m3_renderer_interface *renderer, void *backend,
                                   void *geometry);
void *m3_renderer_create_shader(const m3_renderer_interface *renderer, void *backend,
                                m3_shading_mode mode);
void  m3_renderer_destroy_shader(const m3_renderer_interface *renderer, void *backend,
                                 void *shader);
void  m3_renderer_begin_frame(const m3_renderer_interface *renderer, void *backend,
                              const m3_frame *frame);
void  m3_renderer_submit(const m3_renderer_interface *renderer, void *backend,
                         const m3_frame *frame, const m3_draw_item *draw,
                         void *geometry, void *shader);
void  m3_renderer_end_frame(const m3_renderer_interface *renderer, void *backend,
                             const m3_frame *frame);

#endif /* M3_RENDERER_H */
