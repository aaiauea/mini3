/*
 * m3_light.c - Mini3 lighting model implementation.
 */
#include "mini3/m3_light.h"

void m3_lighting_init(m3_lighting* lighting) {
    if (!lighting) return;
    lighting->directional_count = 0;
    lighting->ambient = M3_VEC3(0.0f, 0.0f, 0.0f);
    for (size_t i = 0; i < M3_MAX_DIRECTIONAL_LIGHTS; i++) {
        lighting->directional[i].direction = M3_VEC3(0.0f, 1.0f, 0.0f);
        lighting->directional[i].color = M3_VEC3(0.0f, 0.0f, 0.0f);
    }
}

void m3_lighting_set_ambient(m3_lighting* lighting, m3_vec3 ambient) {
    if (!lighting) return;
    lighting->ambient = ambient;
}

int m3_lighting_add_directional(m3_lighting* lighting, m3_vec3 direction, m3_vec3 color) {
    if (!lighting || lighting->directional_count >= M3_MAX_DIRECTIONAL_LIGHTS) return -1;
    m3_vec3 dir = direction;
    if (!m3_vec3_normalize(dir, &dir)) dir = M3_VEC3(0.0f, 1.0f, 0.0f);
    m3_directional_light* d = &lighting->directional[lighting->directional_count];
    d->direction = dir;
    d->color = color;
    lighting->directional_count++;
    return 0;
}
