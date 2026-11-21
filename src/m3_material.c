/*
 * m3_material.c - Mini3 material implementation.
 */
#include "mini3/m3_material.h"
#include <stdlib.h>

struct m3_material {
    m3_vec3 diffuse;
    m3_shading_mode mode;
};

m3_material* m3_material_create(m3_vec3 diffuse, m3_shading_mode mode) {
    if (mode != M3_SHADING_FLAT && mode != M3_SHADING_GOURAUD) return NULL;
    m3_material* material = malloc(sizeof(m3_material));
    if (!material) return NULL;
    material->diffuse = diffuse;
    material->mode = mode;
    return material;
}

void m3_material_destroy(m3_material* material) { free(material); }

m3_vec3 m3_material_diffuse(const m3_material* material) {
    return material ? material->diffuse : M3_VEC3(0, 0, 0);
}

m3_shading_mode m3_material_shading(const m3_material* material) {
    return material ? material->mode : M3_SHADING_FLAT;
}

const char* m3_shading_mode_name(m3_shading_mode mode) {
    switch (mode) {
        case M3_SHADING_FLAT: return "flat";
        case M3_SHADING_GOURAUD: return "gouraud";
        default: return "unknown";
    }
}
