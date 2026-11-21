/*
 * m3_mock_backend.h - Mini3 mock/test backend
 *
 * A backend that performs NO rendering work. It records every call the
 * renderer contract makes so tests can assert:
 *   - the expected call sequence happens in a render pass
 *   - resources are created and destroyed exactly once per owner
 *   - the draw list, viewport, and shading modes are what the caller passes
 *
 * It is intentionally a *faithful* implementation of the contract
 * (correct signatures, no GL), so it also serves as a living example of
 * someone implementing the interface.
 */
#ifndef M3_MOCK_BACKEND_H
#define M3_MOCK_BACKEND_H

#include "mini3/m3_renderer.h"

typedef struct {
    /* call counters */
    int set_viewport_count;
    int begin_frame_count;
    int end_frame_count;
    int submit_count;
    int geometry_create_count;
    int geometry_destroy_count;
    int shader_create_count;
    int shader_destroy_count;

    /* viewport */
    int32_t viewport_x, viewport_y, viewport_w, viewport_h;

    /* frame metrics from the last frame */
    size_t last_frame_draw_count;
    float last_frame_view_xx;   /* sample: view matrix element (0,0) */

    /* per-submit record ring buffer (last N submits): index order */
    struct {
        const m3_mesh* mesh;
        m3_shading_mode mode;
        float model_x, model_y, model_z;
    } submits[64];
    int submit_record_count;  /* number of records stored (may exceed 64) */
} m3_mock_backend;

/* The contract implementation. Global, static, no heap. */
extern const m3_renderer_interface m3_mock_renderer;

/* Lifecycle helpers */
m3_mock_backend* m3_mock_backend_create(void *user, const m3_renderer_caps *caps);
void   m3_mock_backend_destroy(m3_mock_backend *backend);

/* Test accessors */
m3_mock_backend* m3_mock_backend_from_backend(const m3_renderer_interface *renderer, void *backend);
void m3_mock_backend_reset(m3_mock_backend *backend);

/* Inspect the most recent submits */
size_t m3_mock_submit_count(const m3_mock_backend *backend);
int    m3_mock_submit_record(const m3_mock_backend *backend, size_t index,
                             m3_shading_mode *mode, float *model_x, float *model_y,
                             float *model_z);

#endif /* M3_MOCK_BACKEND_H */
