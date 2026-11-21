/*
 * test_mock_backend.c - renderer interface conformance tests (mock backend).
 *
 * Verifies that the renderer contract is exercised in the expected order and
 * that resource creation/destroy is exact — without any GPU.
 */
#include <string.h>
#include "test.h"
#include "mini3/m3_renderer.h"
#include "../backends/mock/m3_mock_backend.h"

TEST(mock_renderer_shape) {
    CHECK(m3_mock_renderer.create != NULL);
    CHECK(m3_mock_renderer.destroy != NULL);
    CHECK(m3_mock_renderer.set_viewport != NULL);
    CHECK(m3_mock_renderer.create_geometry != NULL);
    CHECK(m3_mock_renderer.destroy_geometry != NULL);
    CHECK(m3_mock_renderer.create_shader != NULL);
    CHECK(m3_mock_renderer.destroy_shader != NULL);
    CHECK(m3_mock_renderer.begin_frame != NULL);
    CHECK(m3_mock_renderer.submit != NULL);
    CHECK(m3_mock_renderer.end_frame != NULL);
}

TEST(mock_frame_sequence) {
    /* build a scene with 2 drawable nodes and render one frame with the mock */
    m3_scene* scene = m3_scene_create();
    m3_mesh* mesh = m3_mesh_cube(1.0f);
    m3_material* material = m3_material_create(M3_VEC3(0.8f, 0.5f, 0.5f), M3_SHADING_FLAT);
    m3_node* a = m3_scene_add_node(scene, "a");
    m3_node_set_drawable(a, mesh, material);
    m3_node* b = m3_scene_add_node(scene, "b");
    m3_node_set_drawable(b, mesh, material);

    m3_camera* camera = m3_camera_create(60.0f, 1.0f, 0.1f, 100.0f);
    m3_lighting lights;
    m3_lighting_init(&lights);
    m3_lighting_set_ambient(&lights, M3_VEC3(0.2f, 0.2f, 0.25f));

    /* create the mock backend */
    m3_renderer_caps caps = { 0, 4 };
    void* backend = m3_renderer_create(&m3_mock_renderer, NULL, &caps);
    CHECK(backend != NULL);

    /* resources */
    void* geometry = m3_renderer_create_geometry(&m3_mock_renderer, backend, mesh);
    void* shader = m3_renderer_create_shader(&m3_mock_renderer, backend, M3_SHADING_FLAT);
    CHECK(geometry != NULL);
    CHECK(shader != NULL);

    /* viewport */
    CHECK(m3_renderer_set_viewport(&m3_mock_renderer, backend, 0, 0, 128, 96) == 1);

    /* frame */
    m3_frame frame;
    CHECK(m3_frame_build(&frame, scene, camera, &lights) == 1);
    CHECK(frame.draw_count == 2);

    m3_renderer_begin_frame(&m3_mock_renderer, backend, &frame);
    for (size_t i = 0; i < frame.draw_count; i++) {
        m3_renderer_submit(&m3_mock_renderer, backend, &frame, &frame.draws[i], geometry, shader);
    }
    m3_renderer_end_frame(&m3_mock_renderer, backend, &frame);

    /* assert the mock recorded the right things */
    m3_mock_backend* mb = (m3_mock_backend*)backend;
    CHECK(mb->set_viewport_count == 1);
    CHECK(mb->viewport_w == 128);
    CHECK(mb->viewport_h == 96);
    CHECK(mb->begin_frame_count == 1);
    CHECK(mb->end_frame_count == 1);
    CHECK(mb->submit_count == 2);
    CHECK(mb->geometry_create_count == 1);
    CHECK(mb->shader_create_count == 1);
    CHECK(mb->last_frame_draw_count == 2);

    /* submit records: verify mode and model translations of the recorded submits */
    m3_shading_mode rec_mode;
    float rx, ry, rz;
    CHECK(m3_mock_submit_record(mb, 0, &rec_mode, &rx, &ry, &rz) == 0);
    CHECK(rec_mode == M3_SHADING_FLAT);

    /* teardown */
    m3_renderer_destroy_geometry(&m3_mock_renderer, backend, geometry);
    m3_renderer_destroy_shader(&m3_mock_renderer, backend, shader);
    CHECK(mb->geometry_destroy_count == 1);
    CHECK(mb->shader_destroy_count == 1);
    m3_renderer_destroy(&m3_mock_renderer, backend);

    m3_frame_free(&frame);
    m3_camera_destroy(camera);
    m3_material_destroy(material);
    m3_mesh_destroy(mesh);
    m3_scene_destroy(scene);
}

TEST(mock_frame_with_no_draws) {
    /* an empty scene must produce an empty frame and a zero-op pass */
    m3_scene* scene = m3_scene_create();
    m3_camera* camera = m3_camera_create(60.0f, 1.0f, 0.1f, 100.0f);
    m3_frame frame;
    CHECK(m3_frame_build(&frame, scene, camera, NULL) == 1);
    CHECK(frame.draw_count == 0);
    m3_frame_free(&frame);
    m3_camera_destroy(camera);
    m3_scene_destroy(scene);
}

TEST_MAIN(RUN(mock_renderer_shape), RUN(mock_frame_sequence), RUN(mock_frame_with_no_draws));
