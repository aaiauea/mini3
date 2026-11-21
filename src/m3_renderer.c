/*
 * m3_renderer.c - helpers around the m3_renderer_interface vtable.
 */
#include "mini3/m3_renderer.h"

void *m3_renderer_create(const m3_renderer_interface *renderer, void *user,
                         const m3_renderer_caps *caps) {
    if (!renderer || !renderer->create) return NULL;
    return renderer->create(user, caps);
}

void m3_renderer_destroy(const m3_renderer_interface *renderer, void *backend) {
    if (renderer && renderer->destroy) renderer->destroy(backend);
}

bool m3_renderer_set_viewport(const m3_renderer_interface *renderer, void *backend,
                              int32_t x, int32_t y, int32_t w, int32_t h) {
    if (!renderer || !renderer->set_viewport || !backend) return false;
    return renderer->set_viewport(backend, x, y, w, h);
}

void *m3_renderer_create_geometry(const m3_renderer_interface *renderer, void *backend,
                                  const m3_mesh *mesh) {
    if (!renderer || !renderer->create_geometry || !backend) return NULL;
    return renderer->create_geometry(backend, mesh);
}

void m3_renderer_destroy_geometry(const m3_renderer_interface *renderer, void *backend,
                                   void *geometry) {
    if (renderer && renderer->destroy_geometry) renderer->destroy_geometry(backend, geometry);
}

void *m3_renderer_create_shader(const m3_renderer_interface *renderer, void *backend,
                                m3_shading_mode mode) {
    if (!renderer || !renderer->create_shader || !backend) return NULL;
    return renderer->create_shader(backend, mode);
}

void m3_renderer_destroy_shader(const m3_renderer_interface *renderer, void *backend,
                                 void *shader) {
    if (renderer && renderer->destroy_shader) renderer->destroy_shader(backend, shader);
}

void m3_renderer_begin_frame(const m3_renderer_interface *renderer, void *backend,
                              const m3_frame *frame) {
    if (renderer && renderer->begin_frame) renderer->begin_frame(backend, frame);
}

void m3_renderer_submit(const m3_renderer_interface *renderer, void *backend,
                         const m3_frame *frame, const m3_draw_item *draw,
                         void *geometry, void *shader) {
    if (renderer && renderer->submit) renderer->submit(backend, frame, draw, geometry, shader);
}

void m3_renderer_end_frame(const m3_renderer_interface *renderer, void *backend,
                             const m3_frame *frame) {
    if (renderer && renderer->end_frame) renderer->end_frame(backend, frame);
}
