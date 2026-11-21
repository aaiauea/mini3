/*
 * m3_frame.c - Mini3 frame builder (the rendering abstraction).
 *
 * Walks the scene graph, and for each drawable node appends a draw:
 *   { mesh, material, world_matrix }
 * and simultaneously copies the camera view/projection matrices and the
 * lighting model into the frame.
 */
#include "mini3/m3_frame.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    m3_frame* frame;
    int ok;
} m3_frame_builder_ctx;

static int m3_frame_grow(m3_frame* frame, size_t needed) {
    if (needed <= frame->capacity) return 0;
    size_t new_cap = frame->capacity == 0 ? 16 : frame->capacity * 2;
    while (new_cap < needed) new_cap *= 2;
    m3_draw_item* draws = realloc(frame->draws, sizeof(m3_draw_item) * new_cap);
    if (!draws) return -1;
    frame->draws = draws;
    frame->capacity = new_cap;
    return 0;
}

int m3_frame_append(m3_frame* frame, const m3_mesh* mesh, const m3_material* material,
                    m3_mat4 model) {
    if (!frame || !mesh || !material) return -1;
    size_t need = frame->draw_count + 1;
    if (m3_frame_grow(frame, need) != 0) return -1;
    frame->draws[frame->draw_count].mesh = mesh;
    frame->draws[frame->draw_count].material = material;
    frame->draws[frame->draw_count].model = model;
    frame->draw_count++;
    return 0;
}

static void m3_frame_visit(m3_node* node, void* user) {
    /* The frame is built from the scene, so drawable nodes each become draws. */
    m3_frame_builder_ctx* ctx = (m3_frame_builder_ctx*)user;
    m3_mesh* mesh = m3_node_mesh(node);
    m3_material* material = m3_node_material(node);
    if (!mesh || !material) return;
    if (ctx->ok == -1) return;
    m3_mat4 world = m3_node_world(node);
    if (m3_frame_append(ctx->frame, mesh, material, world) != 0) ctx->ok = -1;
}

int m3_frame_build(m3_frame* out, m3_scene* scene, const m3_camera* camera,
                   const m3_lighting* lighting) {
    if (!out || !scene || !camera) return 0;
    memset(out, 0, sizeof(m3_frame));
    m3_frame_builder_ctx ctx = { out, 0 };
    m3_scene_walk(scene, m3_frame_visit, &ctx);
    if (ctx.ok != 0) {
        m3_frame_free(out);
        return 0;
    }
    out->view = m3_camera_view(camera);
    out->proj = m3_camera_projection(camera);
    if (lighting) {
        memcpy(&out->lights, lighting, sizeof(m3_lighting));
    } else {
        m3_lighting_init(&out->lights);
    }
    return 1;
}

void m3_frame_free(m3_frame* frame) {
    if (!frame) return;
    free(frame->draws);
    frame->draws = NULL;
    frame->draw_count = 0;
    frame->capacity = 0;
}
