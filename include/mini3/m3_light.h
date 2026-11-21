/*
 * m3_light.h - Mini3 lighting model
 *
 * Deliberately minimal: a constant ambient tint plus a small fixed set of
 * directional lights. No point lights, no shadows, no PBR — ever (see the
 * project constraints). The whole lighting model travels with the frame.
 */
#ifndef M3_LIGHT_H
#define M3_LIGHT_H

#include "mini3/m3_math.h"
#include <stddef.h>

typedef struct {
    m3_vec3 direction; /* unit vector pointing FROM surfaces TOWARD the light */
    m3_vec3 color;     /* rgb, 0..1 (or HDR-ish intensity) */
} m3_directional_light;

typedef struct {
    size_t directional_count;
    m3_directional_light directional[4]; /* fixed small cap */
    m3_vec3 ambient;                     /* constant multiplier */
} m3_lighting;

#define M3_MAX_DIRECTIONAL_LIGHTS 4

/* Initialize an empty lighting model (ambient 0, no directional lights). */
void m3_lighting_init(m3_lighting* lighting);

/* Set the ambient color. */
void m3_lighting_set_ambient(m3_lighting* lighting, m3_vec3 ambient);

/* Add a directional light (normalizes direction). Returns 0 on success,
 * or if the cap is reached. `direction` is not necessarily pre-normalized. */
int  m3_lighting_add_directional(m3_lighting* lighting, m3_vec3 direction, m3_vec3 color);

#endif /* M3_LIGHT_H */
