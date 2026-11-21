/*
 * test_frame.c - frame builder tests.
 */
#include <string.h>
#include "test.h"
#include "mini3/m3_frame.h"

static void set_node_with_drawable(m3_scene* scene, const char* name, float pos_x, float pos_y, float pos_z,
                                   float yaw_deg, m3_mesh* mesh, m3_material* material) {
    m3_node* node = m3_scene_add_node(scene, name);
    m3_node_set_position(node, M3_VEC3(pos_x, pos_y, pos_z));
    m3_node_set_rotation_euler(node, 0.0f, yaw_deg * M3_PI / 180.0f, 0.0f);
    m3_node_set_drawable(node, mesh, material);
}

TEST(frame_builds_draws_from_scene) {
    m3_scene* scene = m3_scene_create();
    m3_mesh* cube = m3_mesh_cube(1.0f);
    m3_material* material = m3_material_create(M3_VEC3(0.8f, 0.5f, 0.5f), M3_SHADING_FLAT);
    CHECK(cube != NULL && material != NULL);

    set_node_with_drawable(scene, "cube-a", 1.0f, 0.0f, 0.0f, 0.0f, cube, material);
    set_node_with_drawable(scene, "cube-b", 0.0f, 2.0f, 0.0f, 45.0f, cube, material);
    m3_node* empty = m3_scene_add_node(scene, "empty");
    (void)empty; /* no drawable: must not appear in the frame */

    m3_camera* camera = m3_camera_create(60.0f, 1.0f, 0.1f, 100.0f);
    m3_lighting lights;
    m3_lighting_init(&lights);
    m3_lighting_set_ambient(&lights, M3_VEC3(0.2f, 0.2f, 0.25f));

    m3_frame frame;
    int ok = m3_frame_build(&frame, scene, camera, &lights);
    CHECK(ok == 1);
    CHECK(frame.draw_count == 2);

    /* view/proj matrices copied from camera */
    m3_mat4 view = m3_camera_view(camera);
    m3_mat4 proj = m3_camera_projection(camera);
    for (int i = 0; i < 16; i++) {
        CHECK_NEAR(frame.view.m[i], view.m[i], 1e-6f);
        CHECK_NEAR(frame.proj.m[i], proj.m[i], 1e-6f);
    }

    /* lights copied */
    CHECK(frame.lights.directional_count == 0);
    CHECK_NEAR(frame.lights.ambient.x, 0.2f, 1e-4f);

    /* draws: mesh + material + model matrix match */
    m3_node* na = m3_scene_find(scene, "cube-a");
    m3_node* nb = m3_scene_find(scene, "cube-b");
    m3_mat4 expected_a = m3_node_world(na);
    m3_mat4 expected_b = m3_node_world(nb);
    /* order: creation order (na before nb: frame draws appended in walk order — root first, then children in order) */
    CHECK(frame.draws[0].mesh == cube);
    CHECK(frame.draws[0].material == material);
    CHECK_NEAR(frame.draws[0].model.m[12], expected_a.m[12], 1e-4f);
    CHECK_NEAR(frame.draws[1].model.m[12], expected_b.m[12], 1e-4f);

    m3_frame_free(&frame);
    m3_camera_destroy(camera);
    m3_material_destroy(material);
    m3_mesh_destroy(cube);
    m3_scene_destroy(scene);
}

TEST(frame_manual_append) {
    m3_mesh* cube = m3_mesh_cube(1.0f);
    m3_material* material = m3_material_create(M3_VEC3(0.8f, 0.5f, 0.5f), M3_SHADING_GOURAUD);
    CHECK(cube != NULL && material != NULL);

    m3_frame frame;
    memset(&frame, 0, sizeof(frame));
    int ok = m3_frame_append(&frame, cube, material, m3_mat4_identity());
    CHECK(ok == 0);
    CHECK(frame.draw_count == 1);
    CHECK(frame.draws[0].material == material);
    CHECK(frame.draws[0].mesh == cube);
    /* append again */
    ok = m3_frame_append(&frame, cube, material, m3_mat4_identity());
    CHECK(ok == 0);
    CHECK(frame.draw_count == 2);

    m3_frame_free(&frame);
    m3_material_destroy(material);
    m3_mesh_destroy(cube);
}

TEST(frame_free_empty) {
    m3_frame frame;
    memset(&frame, 0, sizeof(frame));
    /* freeing an empty frame must be safe and set storage to NULL */
    m3_frame_free(&frame);
    CHECK(frame.draws == NULL);
    CHECK(frame.draw_count == 0);
}

TEST_MAIN(RUN(frame_builds_draws_from_scene), RUN(frame_manual_append),
          RUN(frame_free_empty));
