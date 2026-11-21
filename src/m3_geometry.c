/*
 * m3_geometry.c - Mini3 mesh construction (see m3_geometry.h).
 */
#include "mini3/m3_geometry.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

struct m3_mesh {
    size_t vertex_count;
    size_t index_count;
    m3_vec3* positions;
    m3_vec3* normals;   /* NULL if not provided */
    float* colors;      /* 3 floats per vertex, NULL if not provided */
    uint32_t* indices;
};

m3_mesh* m3_mesh_create(const m3_mesh_desc* desc) {
    if (!desc || !desc->positions || !desc->indices || desc->vertex_count == 0 || desc->index_count == 0)
        return NULL;
    if (desc->index_count % 3 != 0) return NULL;

    m3_mesh* mesh = calloc(1, sizeof(m3_mesh));
    if (!mesh) return NULL;
    mesh->vertex_count = desc->vertex_count;
    mesh->index_count = desc->index_count;
    mesh->positions = malloc(sizeof(m3_vec3) * desc->vertex_count);
    mesh->indices = malloc(sizeof(uint32_t) * desc->index_count);
    mesh->normals = NULL;
    mesh->colors = NULL;
    if (!mesh->positions || !mesh->indices) { m3_mesh_destroy(mesh); return NULL; }
    memcpy(mesh->positions, desc->positions, sizeof(m3_vec3) * desc->vertex_count);
    memcpy(mesh->indices, desc->indices, sizeof(uint32_t) * desc->index_count);
    if (desc->normals) {
        mesh->normals = malloc(sizeof(m3_vec3) * desc->vertex_count);
        if (!mesh->normals) { m3_mesh_destroy(mesh); return NULL; }
        memcpy(mesh->normals, desc->normals, sizeof(m3_vec3) * desc->vertex_count);
    }
    if (desc->colors) {
        mesh->colors = malloc(sizeof(float) * 3 * desc->vertex_count);
        if (!mesh->colors) { m3_mesh_destroy(mesh); return NULL; }
        memcpy(mesh->colors, desc->colors, sizeof(float) * 3 * desc->vertex_count);
    }
    return mesh;
}

void m3_mesh_destroy(m3_mesh* mesh) {
    if (!mesh) return;
    free(mesh->positions);
    free(mesh->normals);
    free(mesh->colors);
    free(mesh->indices);
    free(mesh);
}

size_t m3_mesh_vertex_count(const m3_mesh* mesh) { return mesh ? mesh->vertex_count : 0; }
size_t m3_mesh_index_count(const m3_mesh* mesh) { return mesh ? mesh->index_count : 0; }
size_t m3_mesh_triangle_count(const m3_mesh* mesh) { return mesh ? mesh->index_count / 3 : 0; }
const m3_vec3* m3_mesh_positions(const m3_mesh* mesh) { return mesh ? mesh->positions : NULL; }
const m3_vec3* m3_mesh_normals(const m3_mesh* mesh) { return mesh ? mesh->normals : NULL; }
const float* m3_mesh_colors(const m3_mesh* mesh) { return mesh ? mesh->colors : NULL; }
const uint32_t* m3_mesh_indices(const m3_mesh* mesh) { return mesh ? mesh->indices : NULL; }

/* ---------------- built-in primitives ---------------- */

/*
 * A cube with 24 vertices (4 per face, per-face duplicated), normals equal to
 * the face normal, per-vertex color = white. 36 indices.
 */
m3_mesh* m3_mesh_cube(float side) {
    float h = side * 0.5f;
    float y = h; /* shorthand */

    /* Face definitions: normal + the 4 corner verts (in local coords). */
    m3_vec3 face_normals[6] = {
        { 1.0f, 0.0f, 0.0f }, { -1.0f, 0.0f, 0.0f },
        { 0.0f, 1.0f, 0.0f }, { 0.0f, -1.0f, 0.0f },
        { 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f, -1.0f }
    };
    /* corners per axis of the face, all unit signs */
    m3_vec3 corners[6][4] = {
        /* +X face */ { { h, -y, -y }, { h, y, -y }, { h, y, y }, { h, -y, y } },
        /* -X face */ { { -h, -y, y }, { -h, y, y }, { -h, y, -y }, { -h, -y, -y } },
        /* +Y face */ { { -h, y, -y }, { -h, y, y }, { h, y, y }, { h, y, -y } },
        /* -Y face */ { { -h, -y, y }, { -h, -y, -y }, { h, -y, -y }, { h, -y, y } },
        /* +Z face */ { { -h, -y, y }, { h, -y, y }, { h, y, y }, { -h, y, y } },
        /* -Z face */ { { h, -y, -y }, { -h, -y, -y }, { -h, y, -y }, { h, y, -y } }
    };

    m3_vec3 positions[24];
    m3_vec3 normals[24];
    float colors[24 * 3];
    for (int f = 0; f < 6; f++) {
        for (int c = 0; c < 4; c++) {
            int v = f * 4 + c;
            positions[v] = corners[f][c];
            normals[v] = face_normals[f];
            colors[v * 3 + 0] = 1.0f;
            colors[v * 3 + 1] = 1.0f;
            colors[v * 3 + 2] = 1.0f;
        }
    }
    uint32_t indices[36];
    for (int f = 0; f < 6; f++) {
        int base = f * 4;
        int i = f * 6;
        indices[i + 0] = base + 0;
        indices[i + 1] = base + 1;
        indices[i + 2] = base + 2;
        indices[i + 3] = base + 0;
        indices[i + 4] = base + 2;
        indices[i + 5] = base + 3;
    }
    m3_mesh_desc desc = {
        .positions = positions,
        .normals = normals,
        .colors = colors,
        .indices = indices,
        .vertex_count = 24,
        .index_count = 36,
    };
    return m3_mesh_create(&desc);
}

m3_mesh* m3_mesh_sphere(float radius, uint32_t rings, uint32_t segments) {
    if (rings < 2 || segments < 3) return NULL;
    uint32_t vcount = (rings + 1) * segments;
    uint32_t icount = rings * segments * 6;
    m3_vec3* positions = malloc(sizeof(m3_vec3) * vcount);
    m3_vec3* normals = malloc(sizeof(m3_vec3) * vcount);
    float* colors = malloc(sizeof(float) * 3 * vcount);
    uint32_t* indices = malloc(sizeof(uint32_t) * icount);
    if (!positions || !normals || !colors || !indices) {
        free(positions); free(normals); free(colors); free(indices);
        return NULL;
    }
    for (uint32_t r = 0; r <= rings; r++) {
        float phi = -M3_PI * 0.5f + (M3_PI * (float)r / (float)rings);
        for (uint32_t s = 0; s < segments; s++) {
            float theta = 2.0f * M3_PI * (float)s / (float)segments;
            float sp = sinf(phi), cp = cosf(phi);
            float st = sinf(theta), ct = cosf(theta);
            m3_vec3 p = M3_VEC3(radius * cp * st, radius * sp, radius * cp * ct);
            uint32_t v = r * segments + s;
            positions[v] = p;
            m3_vec3 n = { p.x / radius, p.y / radius, p.z / radius };
            if (!m3_vec3_normalize(n, &normals[v])) normals[v] = M3_VEC3(0.0f, 1.0f, 0.0f);
            colors[v * 3 + 0] = 1.0f;
            colors[v * 3 + 1] = 1.0f;
            colors[v * 3 + 2] = 1.0f;
        }
    }
    uint32_t i = 0;
    for (uint32_t r = 0; r < rings; r++) {
        for (uint32_t s = 0; s < segments; s++) {
            uint32_t s2 = (s + 1) % segments;
            uint32_t v0 = r * segments + s;
            uint32_t v1 = r * segments + s2;
            uint32_t v2 = (r + 1) * segments + s;
            uint32_t v3 = (r + 1) * segments + s2;
            indices[i++] = v0; indices[i++] = v2; indices[i++] = v1;
            indices[i++] = v1; indices[i++] = v2; indices[i++] = v3;
        }
    }
    m3_mesh_desc desc = {
        .positions = positions,
        .normals = normals,
        .colors = colors,
        .indices = indices,
        .vertex_count = vcount,
        .index_count = icount,
    };
    m3_mesh* mesh = m3_mesh_create(&desc);
    free(positions); free(normals); free(colors); free(indices);
    return mesh;
}

m3_mesh* m3_mesh_plane(float width, float depth) {
    float hw = width * 0.5f, hd = depth * 0.5f;
    m3_vec3 positions[4] = {
        { -hw, 0.0f, -hd }, { hw, 0.0f, -hd }, { hw, 0.0f, hd }, { -hw, 0.0f, hd }
    };
    m3_vec3 normals[4] = {
        { 0.0f, 1.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }
    };
    float colors[4 * 3] = {
        1.0f, 1.0f, 1.0f,  1.0f, 1.0f, 1.0f,  1.0f, 1.0f, 1.0f,  1.0f, 1.0f, 1.0f
    };
    uint32_t indices[6] = { 0, 2, 1, 0, 3, 2 };
    m3_mesh_desc desc = {
        .positions = positions,
        .normals = normals,
        .colors = colors,
        .indices = indices,
        .vertex_count = 4,
        .index_count = 6,
    };
    return m3_mesh_create(&desc);
}
