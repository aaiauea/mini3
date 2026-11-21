/*
 * m3_material.h - Mini3 materials and shading modes
 *
 * A material is a tiny bundle of "look" parameters. Two modes are supported
 * (flat/faceted and Gouraud vertex-lit). Materials carry no shader source or
 * GPU-specific state -- the backend owns shading; the core only passes the
 * mode + diffuse color through the renderer contract.
 */
#ifndef M3_MATERIAL_H
#define M3_MATERIAL_H

#include "mini3/m3_math.h"

typedef enum {
    M3_SHADING_FLAT = 0,   /* faceted: per-face normal */
    M3_SHADING_GOURAUD = 1 /* per-vertex lit, interpolated */
} m3_shading_mode;

typedef struct m3_material m3_material;

/* diffuse: 0..1 RGB. mode: flat or Gouraud. */
m3_material* m3_material_create(m3_vec3 diffuse, m3_shading_mode mode);
void         m3_material_destroy(m3_material* material);

m3_vec3          m3_material_diffuse(const m3_material* material);
m3_shading_mode  m3_material_shading(const m3_material* material);

/* Human-readable name (e.g. "flat", "gouraud") — useful for shader selection. */
const char*      m3_shading_mode_name(m3_shading_mode mode);

#endif /* M3_MATERIAL_H */
