/*
 * m3_frame.h - Mini3 rendering abstractions
 *
 * The "rendering abstraction" is the m3_frame: a plain-data, backend-agnostic
 * description of everything one frame needs to render. The core builds it;
 * the backend consumes it. It contains:
 *
 *   - the view and projection matrices (camera-derived)
 *   - the lighting model (ambient + directional lights)
 *   - the draw list: mesh handle + material handle + world matrix per draw
 *
 * The viewport is intentionally NOT part of the frame — it is a backend
 * setting, configured through the renderer contract.
 *
 * Ownership: m3_frame owns its `draws` array (and only that array); the mesh
 * and material pointers in each draw are borrowed from the scene.
 */
#ifndef M3_FRAME_H
#define M3_FRAME_H

#include "mini3/m3_math.h"
#include "mini3/m3_geometry.h"
#include "mini3/m3_material.h"
#include "mini3/m3_light.h"
#include "mini3/m3_camera.h"
#include "mini3/m3_scene.h"
#include <stddef.h>

typedef struct m3_draw_item {
    const m3_mesh* mesh;     /* borrowed from scene */
    const m3_material* material; /* borrowed from scene */
    m3_mat4 model;           /* world matrix */
} m3_draw_item;

typedef struct m3_frame {
    size_t draw_count;
    size_t capacity;
    m3_draw_item* draws;     /* heap-allocated; NULL if empty */
    m3_mat4 view;
    m3_mat4 proj;
    m3_lighting lights;
} m3_frame;

/* ---- construction ---- */
/* Builds the frame for one render. `lighting` is copied into the frame.
 * Returns 1 on success, 0 if out-of-memory or scene is NULL. */
int m3_frame_build(m3_frame* out, m3_scene* scene, const m3_camera* camera,
                   const m3_lighting* lighting);

/* Destroys the frame's draw storage. Does NOT touch meshes/materials. */
void m3_frame_free(m3_frame* frame);

/* ---- manual construction (for callers that do not use a scene) ---- */
/* Appends a draw with borrowed mesh/material. Returns 0 on success. */
int m3_frame_append(m3_frame* frame, const m3_mesh* mesh, const m3_material* material,
                    m3_mat4 model);

#endif /* M3_FRAME_H */
