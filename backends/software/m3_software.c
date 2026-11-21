/*
 * m3_software.c - Mini3 software reference backend (CPU rasterizer).
 *
 * Pipeline per draw:
 *   composite = proj * view * model (computed once per draw)
 *   per triangle: clip-space transform, view-space normal, flat/gouraud
 *   shading, rasterization with z-buffer
 *
 * Limitations (deliberate, for a reference backend):
 *   - no backface culling or clipping — convex geometry only
 *   - no depth test precision beyond float32
 *   - fixed coordinate space: clip -> ndc -> screen (OpenGL-style, ndc z in [-1,1])
 */
#include "m3_software.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

#define M3_MAX_VERTICES 65536

/* ------------------------------------------------------------------ */
/* Opaque resources                                                    */
/* ------------------------------------------------------------------ */

typedef struct {
    size_t vertex_count;
    size_t index_count;
    m3_vec3* positions;   /* local space */
    m3_vec3* normals;     /* local space, or NULL if none */
    float* colors;        /* 3 per vertex, or NULL if none */
    uint32_t* indices;
} sw_geometry;

typedef struct {
    m3_shading_mode mode;
} sw_shader;

/* ------------------------------------------------------------------ */
/* Backend                                                             */
/* ------------------------------------------------------------------ */

struct m3_software {
    int width;
    int height;
    float clear_r, clear_g, clear_b;
    float* buffer;       /* 3 floats per pixel */
    float* zbuffer;      /* 1 float per pixel; inf = empty */
};

/* ------------------------------------------------------------------ */
/* Resource create/destroy                                             */
/* ------------------------------------------------------------------ */

static sw_geometry* sw_geometry_create(const m3_mesh* mesh) {
    if (!mesh) return NULL;
    size_t vc = m3_mesh_vertex_count(mesh);
    size_t ic = m3_mesh_index_count(mesh);
    if (vc == 0 || ic == 0 || vc > M3_MAX_VERTICES) return NULL;
    sw_geometry* g = malloc(sizeof(sw_geometry));
    if (!g) return NULL;
    g->vertex_count = vc;
    g->index_count = ic;
    g->positions = malloc(sizeof(m3_vec3) * vc);
    g->indices = malloc(sizeof(uint32_t) * ic);
    g->normals = NULL;
    g->colors = NULL;
    if (!g->positions || !g->indices) { free(g->positions); free(g->indices); free(g->normals); free(g->colors); free(g); return NULL; }
    memcpy(g->positions, m3_mesh_positions(mesh), sizeof(m3_vec3) * vc);
    memcpy(g->indices, m3_mesh_indices(mesh), sizeof(uint32_t) * ic);
    const m3_vec3* normals = m3_mesh_normals(mesh);
    if (normals) {
        g->normals = malloc(sizeof(m3_vec3) * vc);
        if (!g->normals) { free(g->positions); free(g->indices); free(g); return NULL; }
        memcpy(g->normals, normals, sizeof(m3_vec3) * vc);
    }
    const float* colors = m3_mesh_colors(mesh);
    if (colors) {
        g->colors = malloc(sizeof(float) * 3 * vc);
        if (!g->colors) { free(g->positions); free(g->normals); free(g->indices); free(g); return NULL; }
        memcpy(g->colors, colors, sizeof(float) * 3 * vc);
    }
    return g;
}

static void sw_geometry_destroy(sw_geometry* g) {
    if (!g) return;
    free(g->positions);
    free(g->normals);
    free(g->colors);
    free(g->indices);
    free(g);
}

/* ------------------------------------------------------------------ */
/* Shading                                                             */
/* ------------------------------------------------------------------ */

/* Per-vertex / per-face lighting contribution: ambient + directional. */
static void sw_light_rgb(const m3_lighting* lights, m3_vec3 n, float* out) {
    float r = lights->ambient.x, g = lights->ambient.y, b = lights->ambient.z;
    for (size_t i = 0; i < lights->directional_count && i < M3_MAX_DIRECTIONAL_LIGHTS; i++) {
        float ndl = m3_vec3_dot(n, lights->directional[i].direction);
        if (ndl > 0.0f) {
            r += ndl * lights->directional[i].color.x;
            g += ndl * lights->directional[i].color.y;
            b += ndl * lights->directional[i].color.z;
        }
    }
    out[0] = r; out[1] = g; out[2] = b;
}

/* Clamp to [0,1]. */
static inline float sw_clamp01(float x) { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); }

/* ------------------------------------------------------------------ */
/* Frame                                                               */
/* ------------------------------------------------------------------ */

static void sw_begin_frame(void* backend_ptr, const m3_frame* frame) {
    m3_software* backend = (m3_software*)backend_ptr;
    if (!backend || !backend->buffer) return;
    (void)frame;
    int n = backend->width * backend->height;
    for (int i = 0; i < n; i++) {
        backend->buffer[i * 3 + 0] = backend->clear_r;
        backend->buffer[i * 3 + 1] = backend->clear_g;
        backend->buffer[i * 3 + 2] = backend->clear_b;
        backend->zbuffer[i] = 2.0f; /* farther than any ndc depth -> empty */
    }
}

/* Clip-space transform via the composite matrix (point, w=1). */
static void sw_clip_transform(m3_mat4 composite, m3_vec3 local, float out[4]) {
    float x = composite.m[0] * local.x + composite.m[4] * local.y + composite.m[8] * local.z + composite.m[12];
    float y = composite.m[1] * local.x + composite.m[5] * local.y + composite.m[9] * local.z + composite.m[13];
    float z = composite.m[2] * local.x + composite.m[6] * local.y + composite.m[10] * local.z + composite.m[14];
    float w = composite.m[3] * local.x + composite.m[7] * local.y + composite.m[11] * local.z + composite.m[15];
    out[0] = x; out[1] = y; out[2] = z; out[3] = w;
}

static void sw_submit(void* backend_ptr, const m3_frame* frame, const m3_draw_item* draw,
                      void* geometry_ptr, void* shader_ptr) {
    m3_software* backend = (m3_software*)backend_ptr;
    sw_geometry* geometry = (sw_geometry*)geometry_ptr;
    sw_shader* shader = (sw_shader*)shader_ptr;
    if (!backend || !frame || !draw || !geometry || !shader) return;
    const m3_material* material = draw->material;
    if (!material) return;
    m3_vec3 diffuse = m3_material_diffuse(material);

    /* composite = proj * view * model */
    m3_mat4 view_model = m3_mat4_multiply(frame->view, draw->model);
    m3_mat4 composite = m3_mat4_multiply(frame->proj, view_model);

    m3_mat4 normal_mat = m3_mat4_normal_matrix(draw->model);

    int w = backend->width, h = backend->height;
    if (w <= 0 || h <= 0) return;

    size_t vc = geometry->vertex_count;
    /* clip-space per-vertex transform (points, w=1) */
    float (*clips)[4] = malloc(sizeof(float[4]) * vc);
    if (!clips) return;
    for (size_t i = 0; i < vc; i++) {
        sw_clip_transform(composite, geometry->positions[i], clips[i]);
    }

    for (size_t t = 0; t < geometry->index_count; t += 3) {
        uint32_t i0 = geometry->indices[t];
        uint32_t i1 = geometry->indices[t + 1];
        uint32_t i2 = geometry->indices[t + 2];
        if (i0 >= vc || i1 >= vc || i2 >= vc) continue;
        if (clips[i0][3] <= 0.0f || clips[i1][3] <= 0.0f || clips[i2][3] <= 0.0f) continue;

        /* clip-space w per vertex, ndc */
        float clip_w[3] = { clips[i0][3], clips[i1][3], clips[i2][3] };
        float ndc_x[3] = { clips[i0][0] / clips[i0][3], clips[i1][0] / clips[i1][3], clips[i2][0] / clips[i2][3] };
        float ndc_y[3] = { clips[i0][1] / clips[i0][3], clips[i1][1] / clips[i1][3], clips[i2][1] / clips[i2][3] };
        float ndc_z[3] = { clips[i0][2] / clips[i0][3], clips[i1][2] / clips[i1][3], clips[i2][2] / clips[i2][3] };

        /* screen coords */
        float sx[3] = { (ndc_x[0] * 0.5f + 0.5f) * (float)w, (ndc_x[1] * 0.5f + 0.5f) * (float)w, (ndc_x[2] * 0.5f + 0.5f) * (float)w };
        float sy[3] = { (1.0f - (ndc_y[0] * 0.5f + 0.5f)) * (float)h, (1.0f - (ndc_y[1] * 0.5f + 0.5f)) * (float)h, (1.0f - (ndc_y[2] * 0.5f + 0.5f)) * (float)h };

        int minx = (int)floorf(fminf(fminf(sx[0], sx[1]), sx[2]));
        int maxx = (int)ceilf(fmaxf(fmaxf(sx[0], sx[1]), sx[2]));
        int miny = (int)floorf(fminf(fminf(sy[0], sy[1]), sy[2]));
        int maxy = (int)ceilf(fmaxf(fmaxf(sy[0], sy[1]), sy[2]));
        if (maxx < 0 || minx >= w || maxy < 0 || miny >= h) continue;
        if (minx < 0) minx = 0;
        if (maxx >= w) maxx = w - 1;
        if (miny < 0) miny = 0;
        if (maxy >= h) maxy = h - 1;

        /* edge vectors for barycentric */
        float v0x = sx[1] - sx[0], v0y = sy[1] - sy[0];
        float v1x = sx[2] - sx[0], v1y = sy[2] - sy[0];
        float dot00 = v0x * v0x + v0y * v0y;
        float dot01 = v0x * v1x + v0y * v1y;
        float dot11 = v1x * v1x + v1y * v1y;
        float denom = dot00 * dot11 - dot01 * dot01;
        if (fabsf(denom) < 1e-12f) continue;

        /* per-vertex shading colors */
        float vcolor[3][3];
        if (shader->mode == M3_SHADING_FLAT) {
            m3_vec3 local_pos[3] = { geometry->positions[i0], geometry->positions[i1], geometry->positions[i2] };
            m3_vec3 edge1 = m3_vec3_sub(local_pos[1], local_pos[0]);
            m3_vec3 edge2 = m3_vec3_sub(local_pos[2], local_pos[0]);
            m3_vec3 local_normal = m3_vec3_cross(edge1, edge2);
            m3_vec3 world_normal = m3_mat4_transform_direction(normal_mat, local_normal);
            m3_vec3 n;
            if (!m3_vec3_normalize(world_normal, &n)) n = M3_VEC3(0.0f, 1.0f, 0.0f);
            float intensity[3];
            sw_light_rgb(&frame->lights, n, intensity);
            for (int k = 0; k < 3; k++) {
                vcolor[k][0] = diffuse.x * intensity[0];
                vcolor[k][1] = diffuse.y * intensity[1];
                vcolor[k][2] = diffuse.z * intensity[2];
            }
        } else {
            for (int k = 0; k < 3; k++) {
                int idx = (k == 0) ? i0 : (k == 1) ? i1 : i2;
                const m3_vec3* local_normal_s = geometry->normals ? &geometry->normals[idx] : NULL;
                m3_vec3 local_normal;
                if (local_normal_s) {
                    local_normal = *local_normal_s;
                } else {
                    m3_vec3 local_pos0 = geometry->positions[i0];
                    m3_vec3 local_pos1 = geometry->positions[i1];
                    m3_vec3 local_pos2 = geometry->positions[i2];
                    m3_vec3 edge1 = m3_vec3_sub(local_pos1, local_pos0);
                    m3_vec3 edge2 = m3_vec3_sub(local_pos2, local_pos0);
                    local_normal = m3_vec3_cross(edge1, edge2);
                }
                m3_vec3 world_normal = m3_mat4_transform_direction(normal_mat, local_normal);
                m3_vec3 n;
                if (!m3_vec3_normalize(world_normal, &n)) n = M3_VEC3(0.0f, 1.0f, 0.0f);
                float intensity[3];
                sw_light_rgb(&frame->lights, n, intensity);
                float vcolor_rgb[3] = { 1.0f, 1.0f, 1.0f };
                if (geometry->colors) {
                    vcolor_rgb[0] = geometry->colors[idx * 3 + 0];
                    vcolor_rgb[1] = geometry->colors[idx * 3 + 1];
                    vcolor_rgb[2] = geometry->colors[idx * 3 + 2];
                }
                vcolor[k][0] = diffuse.x * intensity[0] * vcolor_rgb[0];
                vcolor[k][1] = diffuse.y * intensity[1] * vcolor_rgb[1];
                vcolor[k][2] = diffuse.z * intensity[2] * vcolor_rgb[2];
            }
        }

        /* perspective-correct depth interpolation */
        float inv_w[3] = { 1.0f / clip_w[0], 1.0f / clip_w[1], 1.0f / clip_w[2] };

        for (int y = miny; y <= maxy; y++) {
            for (int x = minx; x <= maxx; x++) {
                float px = (float)x + 0.5f;
                float py = (float)y + 0.5f;
                float v2x = px - sx[0], v2y = py - sy[0];
                float dot02 = v0x * v2x + v0y * v2y;
                float dot12 = v1x * v2x + v1y * v2y;
                float lw1 = (dot11 * dot02 - dot01 * dot12) / denom;
                float lw2 = (dot00 * dot12 - dot01 * dot02) / denom;
                float lw0 = 1.0f - lw1 - lw2;
                if (lw0 < -0.001f || lw1 < -0.001f || lw2 < -0.001f) continue;

                /* per-pixel color (Gouraud: plain screen-space barycentric) */
                float cr = vcolor[0][0] * lw0 + vcolor[1][0] * lw1 + vcolor[2][0] * lw2;
                float cg = vcolor[0][1] * lw0 + vcolor[1][1] * lw1 + vcolor[2][1] * lw2;
                float cb = vcolor[0][2] * lw0 + vcolor[1][2] * lw1 + vcolor[2][2] * lw2;

                /* perspective-correct depth */
                float num = 0.0f, den = 0.0f;
                for (int k = 0; k < 3; k++) {
                    float l = (k == 0) ? lw0 : (k == 1) ? lw1 : lw2;
                    num += l * ndc_z[k] * inv_w[k];
                    den += l * inv_w[k];
                }
                float depth = den > 1e-12f ? num / den : (ndc_z[0] * lw0 + ndc_z[1] * lw1 + ndc_z[2] * lw2);

                int pixel_idx = y * w + x;
                if (depth < backend->zbuffer[pixel_idx]) {
                    backend->zbuffer[pixel_idx] = depth;
                    backend->buffer[pixel_idx * 3 + 0] = sw_clamp01(cr);
                    backend->buffer[pixel_idx * 3 + 1] = sw_clamp01(cg);
                    backend->buffer[pixel_idx * 3 + 2] = sw_clamp01(cb);
                }
            }
        }
    }
    free(clips);
}

/* ------------------------------------------------------------------ */
/* Frame management                                                    */
/* ------------------------------------------------------------------ */

static void sw_end_frame(void* backend_ptr, const m3_frame* frame) {
    (void)backend_ptr;
    (void)frame;
    /* nothing to finalize: the framebuffer is valid for inspection/export */
    (void)0;
}

/* ------------------------------------------------------------------ */
/* Lifecycle                                                           */
/* ------------------------------------------------------------------ */

static void* sw_create(void* user, const m3_renderer_caps* caps) {
    (void)user; (void)caps;
    m3_software* backend = calloc(1, sizeof(m3_software));
    if (!backend) return NULL;
    backend->width = 0;
    backend->height = 0;
    backend->clear_r = 0.03f;
    backend->clear_g = 0.03f;
    backend->clear_b = 0.04f;
    backend->buffer = NULL;
    backend->zbuffer = NULL;
    return (void*)backend;
}

static void sw_destroy(void* backend_ptr) {
    m3_software* backend = (m3_software*)backend_ptr;
    if (!backend) return;
    free(backend->buffer);
    free(backend->zbuffer);
    free(backend);
}

static bool sw_set_viewport(void* backend_ptr, int32_t x, int32_t y, int32_t w, int32_t h) {
    (void)x; (void)y;
    if (w <= 0 || h <= 0 || w > 8192 || h > 8192) return false;
    m3_software* backend = (m3_software*)backend_ptr;
    if (!backend) return false;
    int n = w * h;
    float* buffer = malloc(sizeof(float) * 3 * n);
    float* zbuffer = malloc(sizeof(float) * n);
    if (!buffer || !zbuffer) { free(buffer); free(zbuffer); return false; }
    free(backend->buffer);
    free(backend->zbuffer);
    backend->buffer = buffer;
    backend->zbuffer = zbuffer;
    backend->width = w;
    backend->height = h;
    return true;
}

static void* sw_create_geometry(void* backend_ptr, const m3_mesh* mesh) {
    (void)backend_ptr;
    return sw_geometry_create(mesh);
}

static void sw_destroy_geometry(void* backend_ptr, void* geometry) {
    (void)backend_ptr;
    sw_geometry_destroy((sw_geometry*)geometry);
}

static void* sw_create_shader(void* backend_ptr, m3_shading_mode mode) {
    (void)backend_ptr;
    if (mode != M3_SHADING_FLAT && mode != M3_SHADING_GOURAUD) return NULL;
    sw_shader* shader = malloc(sizeof(sw_shader));
    if (!shader) return NULL;
    shader->mode = mode;
    return shader;
}

static void sw_destroy_shader(void* backend_ptr, void* shader) {
    (void)backend_ptr;
    free(shader);
}

/* ------------------------------------------------------------------ */
/* Interface                                                           */
/* ------------------------------------------------------------------ */

const m3_renderer_interface m3_software_renderer = {
    .create = sw_create,
    .destroy = sw_destroy,
    .set_viewport = sw_set_viewport,
    .create_geometry = sw_create_geometry,
    .destroy_geometry = sw_destroy_geometry,
    .create_shader = sw_create_shader,
    .destroy_shader = sw_destroy_shader,
    .begin_frame = sw_begin_frame,
    .submit = sw_submit,
    .end_frame = sw_end_frame,
};

const m3_renderer_interface* m3_software_renderer_get_interface(void) {
    return &m3_software_renderer;
}

/* ------------------------------------------------------------------ */
/* Public helpers                                                      */
/* ------------------------------------------------------------------ */

m3_software* m3_software_create(void) { return (m3_software*)sw_create(NULL, NULL); }
void m3_software_destroy(m3_software* backend) { sw_destroy(backend); }

size_t m3_software_viewport_size(const m3_software* backend, int* w, int* h) {
    if (!backend) return 0;
    if (w) *w = backend->width;
    if (h) *h = backend->height;
    return (size_t)backend->width * backend->height;
}

int m3_software_get_pixel(const m3_software* backend, int x, int y, float rgb[3]) {
    if (!backend || x < 0 || y < 0 || x >= backend->width || y >= backend->height) return 0;
    int idx = y * backend->width + x;
    rgb[0] = backend->buffer[idx * 3 + 0];
    rgb[1] = backend->buffer[idx * 3 + 1];
    rgb[2] = backend->buffer[idx * 3 + 2];
    return 1;
}

size_t m3_software_nontrivial_pixel_count(const m3_software* backend) {
    if (!backend || !backend->buffer) return 0;
    size_t count = 0;
    int n = backend->width * backend->height;
    for (int i = 0; i < n; i++) {
        float r = backend->buffer[i * 3 + 0];
        float g = backend->buffer[i * 3 + 1];
        float b = backend->buffer[i * 3 + 2];
        if (fabsf(r - backend->clear_r) > 0.02f || fabsf(g - backend->clear_g) > 0.02f ||
            fabsf(b - backend->clear_b) > 0.02f)
            count++;
    }
    return count;
}

int m3_software_write_ppm(const m3_software* backend, const char* path) {
    if (!backend || !path || !backend->buffer) return 0;
    FILE* f = fopen(path, "wb");
    if (!f) return 0;
    fprintf(f, "P6\n%d %d\n255\n", backend->width, backend->height);
    int n = backend->width * backend->height;
    for (int i = 0; i < n; i++) {
        uint8_t r = (uint8_t)(sw_clamp01(backend->buffer[i * 3 + 0]) * 255.0f);
        uint8_t g = (uint8_t)(sw_clamp01(backend->buffer[i * 3 + 1]) * 255.0f);
        uint8_t b = (uint8_t)(sw_clamp01(backend->buffer[i * 3 + 2]) * 255.0f);
        fwrite(&r, 1, 1, f);
        fwrite(&g, 1, 1, f);
        fwrite(&b, 1, 1, f);
    }
    fclose(f);
    return 1;
}
