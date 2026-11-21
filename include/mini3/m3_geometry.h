/*
 * m3_geometry.h - Mini3 geometry (meshes)
 *
 * A mesh is a dumb container of vertex data, ownership side: the mesh owns
 * its internal arrays. Meshes carry up to four optional layers
 * (positions, per-vertex normals, per-vertex colors, indices). Backends are
 * handed m3_mesh directly -- the core never knows what a backend will do
 * with it.
 *
 * Shading modes consume: positions always, normals & colors when present.
 * Faceted ("flat") shading in a backend typically re-derives the face normal
 * from positions and ignores per-vertex normals; Gouraud uses normals and
 * colors for per-vertex lighting.
 */
#ifndef M3_GEOMETRY_H
#define M3_GEOMETRY_H

#include "mini3/m3_math.h"
#include <stdint.h>
#include <stddef.h>

typedef struct m3_mesh m3_mesh;

/* Metadata descriptor; the mesh copies the arrays. All layers are optional
 * except positions and indices. */
typedef struct {
    const m3_vec3* positions;    /* vertex positions; required */
    const float* colors;         /* per-vertex rgb (3 per vertex); optional */
    const m3_vec3* normals;      /* per-vertex normals; optional */
    const uint32_t* indices;     /* triangle indices; required */
    size_t vertex_count;
    size_t index_count;          /* should be a multiple of 3 */
} m3_mesh_desc;

/* Mesh lifecycle. Both may be NULL for no mesh/material. */
m3_mesh* m3_mesh_create(const m3_mesh_desc* desc);
void     m3_mesh_destroy(m3_mesh* mesh);

/* Accessors (const views into the mesh — do not free or mutate). */
size_t   m3_mesh_vertex_count(const m3_mesh* mesh);
size_t   m3_mesh_index_count(const m3_mesh* mesh);
size_t   m3_mesh_triangle_count(const m3_mesh* mesh);
const m3_vec3*      m3_mesh_positions(const m3_mesh* mesh);
const m3_vec3*      m3_mesh_normals(const m3_mesh* mesh); /* NULL if none */
const float*        m3_mesh_colors(const m3_mesh* mesh);  /* NULL if none */
const uint32_t*     m3_mesh_indices(const m3_mesh* mesh);

/* ---- built-in primitives ----
 * These build the mesh via the same m3_mesh machinery so backends treat
 * them like user geometry. */

/* Cube, side length `side` centered on origin, per-face split vertices
 * (24 verts / 36 indices) so flat shading works per face and per-vertex
 * normals are exact per face. */
m3_mesh* m3_mesh_cube(float side);

/* Sphere with `rings` elevation bands x `segments` radial segments,
 * per-vertex normals, white per-vertex color. */
m3_mesh* m3_mesh_sphere(float radius, uint32_t rings, uint32_t segments);

/* Triangulated flat plane in the XZ plane (centered at origin), `width` x `depth`. */
m3_mesh* m3_mesh_plane(float width, float depth);

#endif /* M3_GEOMETRY_H */
