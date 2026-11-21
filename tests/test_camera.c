/*
 * test_camera.c - camera tests.
 */
#include "test.h"
#include "mini3/m3_camera.h"

TEST(camera_create_destroy) {
    m3_camera* camera = m3_camera_create(60.0f, 16.0f / 9.0f, 0.1f, 100.0f);
    CHECK(camera != NULL);
    CHECK(m3_camera_fov(camera) == 60.0f);
    CHECK_NEAR(m3_camera_aspect(camera), 16.0f / 9.0f, 1e-5f);
    m3_camera_destroy(camera);
}

TEST(camera_view_identity_at_default) {
    /* camera at origin with identity rotation: view = identity */
    m3_camera* camera = m3_camera_create(60.0f, 1.0f, 0.1f, 100.0f);
    m3_camera_set_position(camera, M3_VEC3(0.0f, 0.0f, 0.0f));
    m3_camera_set_rotation(camera, m3_quat_identity());
    m3_mat4 view = m3_camera_view(camera);
    m3_mat4 id = m3_mat4_identity();
    for (int i = 0; i < 16; i++) CHECK_NEAR(view.m[i], id.m[i], 1e-4f);
    m3_camera_destroy(camera);
}

TEST(camera_view_translation) {
    /* camera translated +5 in x, no rotation: view = translation(-5,0,0) */
    m3_camera* camera = m3_camera_create(60.0f, 1.0f, 0.1f, 100.0f);
    m3_camera_set_position(camera, M3_VEC3(5.0f, 0.0f, 0.0f));
    m3_camera_set_rotation(camera, m3_quat_identity());
    m3_mat4 view = m3_camera_view(camera);
    /* a point at world (5,0,0) should land at view origin */
    m3_vec3 p = m3_mat4_transform_point(view, M3_VEC3(5.0f, 0.0f, 0.0f));
    CHECK_NEAR(p.x, 0.0f, 1e-3f);
    m3_vec3 p2 = m3_mat4_transform_point(view, M3_VEC3(5.0f, 1.0f, 2.0f));
    CHECK_NEAR(p2.x, 0.0f, 1e-3f);
    CHECK_NEAR(p2.y, 1.0f, 1e-3f);
    CHECK_NEAR(p2.z, 2.0f, 1e-3f);
    m3_camera_destroy(camera);
}

TEST(camera_view_rotation) {
    /* camera rotated 90deg about Y: world point +X lands toward -Z in view space */
    m3_camera* camera = m3_camera_create(60.0f, 1.0f, 0.1f, 100.0f);
    m3_camera_set_position(camera, M3_VEC3(0.0f, 0.0f, 0.0f));
    m3_camera_set_rotation(camera, m3_quat_from_euler_deg(0.0f, 90.0f, 0.0f));
    m3_mat4 view = m3_camera_view(camera);
    /* a world point at (1,0,0) (world X): camera looks down -z; world X is
       rotated so it lands at view Z = -1 for 90deg yaw? verify via direct check */
    m3_vec3 p = m3_mat4_transform_point(view, M3_VEC3(1.0f, 0.0f, 0.0f));
    /* view space +X for an identity-positioned, yaw-90 camera: world +X
       should be at view -Z? compute: view applies R^T. R(90y) * world_X → */
    float exp_x = cosf(1.5707963267948966f) * 1.0f;
    float exp_z = -sinf(1.5707963267948966f) * 1.0f;
    CHECK_NEAR(p.x, exp_x, 1e-2f);
    CHECK_NEAR(p.z, -exp_z, 1e-2f);
    m3_camera_destroy(camera);
}

TEST(camera_view_matches_inverse) {
    /* view must equal inverse(T*R) exactly */
    m3_camera* camera = m3_camera_create(60.0f, 1.5f, 0.1f, 100.0f);
    m3_camera_set_position(camera, M3_VEC3(1.5f, -2.0f, 3.0f));
    m3_camera_set_rotation(camera, m3_quat_from_euler_deg(20.0f, -40.0f, 10.0f));
    m3_mat4 view = m3_camera_view(camera);

    m3_mat4 world = m3_mat4_compose(
        m3_camera_position(camera), m3_camera_rotation(camera), M3_VEC3(1, 1, 1));
    m3_mat4 inverse_world;
    CHECK(m3_mat4_inverse(world, &inverse_world) == 1);
    for (int i = 0; i < 16; i++) CHECK_NEAR(view.m[i], inverse_world.m[i], 1e-3f);
    m3_camera_destroy(camera);
}

TEST(camera_projection) {
    m3_camera* camera = m3_camera_create(45.0f, 2.0f, 0.1f, 100.0f);
    m3_mat4 proj = m3_camera_projection(camera);
    /* projection at x=... check m.m[0] = f/aspect, m.m[5] = f */
    float f = 1.0f / tanf((45.0f * M3_PI / 180.0f) * 0.5f);
    CHECK_NEAR(proj.m[0], f / 2.0f, 1e-3f);
    CHECK_NEAR(proj.m[5], f, 1e-3f);
    m3_camera_destroy(camera);
}

TEST(camera_look_at) {
    m3_camera* camera = m3_camera_create(60.0f, 1.0f, 0.1f, 100.0f);
    m3_camera_look_at(camera, M3_VEC3(0.0f, 1.2f, 4.0f), M3_VEC3(0.0f, 0.0f, 0.0f),
                      M3_VEC3(0.0f, 1.0f, 0.0f));
    m3_mat4 view = m3_camera_view(camera);
    m3_mat4 expected = m3_mat4_look_at(M3_VEC3(0.0f, 1.2f, 4.0f), M3_VEC3(0.0f, 0.0f, 0.0f),
                                      M3_VEC3(0.0f, 1.0f, 0.0f));
    for (int i = 0; i < 16; i++) CHECK_NEAR(view.m[i], expected.m[i], 1e-3f);
    m3_camera_destroy(camera);
}

TEST_MAIN(RUN(camera_create_destroy), RUN(camera_look_at), RUN(camera_view_identity_at_default),
          RUN(camera_view_translation), RUN(camera_view_rotation),
          RUN(camera_view_matches_inverse), RUN(camera_projection));
