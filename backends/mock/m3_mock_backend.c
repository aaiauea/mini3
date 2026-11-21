/*
 * m3_mock_backend.c - Mini3 mock/test backend (records calls, renders nothing).
 */
#include "m3_mock_backend.h"
#include <stdlib.h>
#include <string.h>

/* Opaque resource token handed to backends that need per-resource identity. */
typedef struct { int token; } mock_resource;

/* The m3_mock_backend struct is defined in m3_mock_backend.h; here we add the
 * opaque resource tokens. */

static void* mock_create(void* user, const m3_renderer_caps *caps) {
    (void)user; (void)caps;
    m3_mock_backend* backend = calloc(1, sizeof(m3_mock_backend));
    if (!backend) return NULL;
    backend->submit_record_count = 0;
    return backend;
}

static void mock_destroy(void* backend) { free(backend); }

static bool mock_set_viewport(void* backend, int32_t x, int32_t y, int32_t w, int32_t h) {
    m3_mock_backend* b = (m3_mock_backend*)backend;
    if (!b) return false;
    b->set_viewport_count++;
    b->viewport_x = x; b->viewport_y = y;
    b->viewport_w = w; b->viewport_h = h;
    return true;
}

static void* mock_create_geometry(void* backend, const m3_mesh* mesh) {
    m3_mock_backend* b = (m3_mock_backend*)backend;
    if (!b || !mesh) return NULL;
    mock_resource* resource = malloc(sizeof(mock_resource));
    if (!resource) return NULL;
    resource->token = 0x1000 + b->geometry_create_count + 1;
    b->geometry_create_count++;
    return resource;
}

static void mock_destroy_geometry(void* backend, void* geometry) {
    m3_mock_backend* b = (m3_mock_backend*)backend;
    if (!b) return;
    if (geometry) { b->geometry_destroy_count++; free(geometry); }
}

static void* mock_create_shader(void* backend, m3_shading_mode mode) {
    (void)mode;
    m3_mock_backend* b = (m3_mock_backend*)backend;
    if (!b) return NULL;
    mock_resource* resource = malloc(sizeof(mock_resource));
    if (!resource) return NULL;
    resource->token = 0x2000 + b->shader_create_count + 1;
    b->shader_create_count++;
    return resource;
}

static void mock_destroy_shader(void* backend, void* shader) {
    m3_mock_backend* b = (m3_mock_backend*)backend;
    if (!b) return;
    if (shader) { b->shader_destroy_count++; free(shader); }
}

static void mock_begin_frame(void* backend, const m3_frame* frame) {
    m3_mock_backend* b = (m3_mock_backend*)backend;
    if (!b || !frame) return;
    b->begin_frame_count++;
    b->last_frame_draw_count = frame->draw_count;
    b->last_frame_view_xx = frame->view.m[0];
}

static void mock_submit(void* backend, const m3_frame* frame, const m3_draw_item* draw,
                        void* geometry, void* shader) {
    (void)frame; (void)geometry; (void)shader;
    m3_mock_backend* b = (m3_mock_backend*)backend;
    if (!b || !draw) return;
    b->submit_count++;
    if (b->submit_record_count < 64) {
        b->submits[b->submit_record_count].mesh = draw->mesh;
        b->submits[b->submit_record_count].mode = m3_material_shading(draw->material);
        b->submits[b->submit_record_count].model_x = draw->model.m[12];
        b->submits[b->submit_record_count].model_y = draw->model.m[13];
        b->submits[b->submit_record_count].model_z = draw->model.m[14];
        b->submit_record_count++;
    }
}

static void mock_end_frame(void* backend, const m3_frame* frame) {
    (void)frame;
    m3_mock_backend* b = (m3_mock_backend*)backend;
    if (!b) return;
    b->end_frame_count++;
}

const m3_renderer_interface m3_mock_renderer = {
    .create = mock_create,
    .destroy = mock_destroy,
    .set_viewport = mock_set_viewport,
    .create_geometry = mock_create_geometry,
    .destroy_geometry = mock_destroy_geometry,
    .create_shader = mock_create_shader,
    .destroy_shader = mock_destroy_shader,
    .begin_frame = mock_begin_frame,
    .submit = mock_submit,
    .end_frame = mock_end_frame,
};

m3_mock_backend* m3_mock_backend_create(void *user, const m3_renderer_caps *caps) {
    return (m3_mock_backend*)m3_mock_renderer.create(user, caps);
}

void m3_mock_backend_destroy(m3_mock_backend *backend) {
    if (backend) m3_mock_renderer.destroy(backend);
}

void m3_mock_backend_reset(m3_mock_backend *backend) {
    if (backend) memset(backend, 0, sizeof(m3_mock_backend));
}

m3_mock_backend* m3_mock_backend_from_backend(const m3_renderer_interface *renderer, void *backend) {
    (void)renderer;
    return (m3_mock_backend*)backend;
}

size_t m3_mock_submit_count(const m3_mock_backend *backend) {
    return backend ? (size_t)backend->submit_count : 0;
}

int m3_mock_submit_record(const m3_mock_backend *backend, size_t index,
                         m3_shading_mode *mode, float *model_x, float *model_y,
                         float *model_z) {
    if (!backend || index >= (size_t)backend->submit_record_count) return -1;
    if (mode) *mode = backend->submits[index].mode;
    if (model_x) *model_x = backend->submits[index].model_x;
    if (model_y) *model_y = backend->submits[index].model_y;
    if (model_z) *model_z = backend->submits[index].model_z;
    return 0;
}
