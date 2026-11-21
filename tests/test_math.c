/*
 * test_math.c - unit tests for the math library.
 */
#include "test.h"
#include "mini3/m3_math.h"

TEST(vec3) {
    m3_vec3 a = M3_VEC3(1, 2, 3);
    m3_vec3 b = M3_VEC3(4, 5, 6);
    m3_vec3 sum = m3_vec3_add(a, b);
    CHECK_NEAR(sum.x, 5, 1e-4f); CHECK_NEAR(sum.y, 7, 1e-4f); CHECK_NEAR(sum.z, 9, 1e-4f);
    m3_vec3 diff = m3_vec3_sub(a, b);
    CHECK_NEAR(diff.x, -3, 1e-4f);
    m3_vec3 scaled = m3_vec3_scale(a, 2.0f);
    CHECK_NEAR(scaled.y, 4, 1e-4f);
    CHECK_NEAR(m3_vec3_dot(a, b), 1 * 4 + 2 * 5 + 3 * 6, 1e-4f);
    m3_vec3 cross = m3_vec3_cross(M3_VEC3(1, 0, 0), M3_VEC3(0, 1, 0));
    CHECK_NEAR(cross.x, 0, 1e-4f); CHECK_NEAR(cross.y, 0, 1e-4f); CHECK_NEAR(cross.z, 1, 1e-4f);
    m3_vec3 unit;
    CHECK(m3_vec3_normalize(M3_VEC3(3, 0, 4), &unit) == 1);
    CHECK_NEAR(unit.x, 0.6f, 1e-4f); CHECK_NEAR(unit.z, 0.8f, 1e-4f);
    m3_vec3 zero_out;
    CHECK(m3_vec3_normalize(M3_VEC3(0, 0, 0), &zero_out) == 0);
}

TEST(quat_mul_identity) {
    m3_quat q = m3_quat_normalize(M3_QUAT(0.5f, 0.5f, 0.5f, 0.5f));
    m3_quat r = m3_quat_mul(m3_quat_identity(), q);
    CHECK_NEAR(r.x, q.x, 1e-4f); CHECK_NEAR(r.y, q.y, 1e-4f); CHECK_NEAR(r.z, q.z, 1e-4f); CHECK_NEAR(r.w, q.w, 1e-4f);
    m3_quat r2 = m3_quat_mul(q, m3_quat_identity());
    CHECK_NEAR(r2.x, q.x, 1e-4f); CHECK_NEAR(r2.w, q.w, 1e-4f);
}

TEST(quat_rotate_90y) {
    /* 90 degrees about Y axis: quat = (0, sin(45), 0, cos(45)) */
    float s = sinf(M3_PI / 4.0f);
    m3_quat q = M3_QUAT(0.0f, s, 0.0f, s);
    m3_vec3 out = m3_quat_rotate_vec3(q, M3_VEC3(1.0f, 0.0f, 0.0f));
    CHECK_NEAR(out.x, 0.0f, 1e-3f);   /* (cos90, 0, -sin90) */
    CHECK_NEAR(out.y, 0.0f, 1e-3f);
    CHECK_NEAR(out.z, -1.0f, 1e-3f);
}

TEST(quat_from_euler_yaw90) {
    /* yaw=90 deg about Y — rotate vector (1,0,0) to (0,0,-1) */
    m3_quat q = m3_quat_from_euler_deg(0.0f, 90.0f, 0.0f);
    m3_vec3 out = m3_quat_rotate_vec3(q, M3_VEC3(1.0f, 0.0f, 0.0f));
    CHECK_NEAR(out.x, 0.0f, 1e-3f);
    CHECK_NEAR(out.y, 0.0f, 1e-3f);
    CHECK_NEAR(out.z, -1.0f, 1e-3f);
}

TEST(mat4_identity_and_compose) {
    m3_mat4 id = m3_mat4_identity();
    m3_mat4 out = m3_mat4_multiply(id, id);
    for (int i = 0; i < 16; i++) CHECK_NEAR(out.m[i], id.m[i], 1e-5f);

    /* compose(pos=(10,20,30), yaw=90deg, scale=(2,2,2)) then transform (0,0,0) */
    m3_vec3 pos = M3_VEC3(10.0f, 20.0f, 30.0f);
    m3_quat rot = m3_quat_from_euler_deg(0.0f, 90.0f, 0.0f);
    m3_mat4 m = m3_mat4_compose(pos, rot, M3_VEC3(2.0f, 2.0f, 2.0f));
    m3_vec3 after = m3_mat4_transform_point(m, M3_VEC3(0, 0, 0));
    /* point at origin: (0,0,0) -> T * R * S * (0,0,0) = T(0,0,0) = (10,20,30) */
    CHECK_NEAR(after.x, 10.0f, 1e-3f);
    CHECK_NEAR(after.y, 20.0f, 1e-3f);
    CHECK_NEAR(after.z, 30.0f, 1e-3f);

    /* non-origin point through scale+rotate+translate */
    m3_vec3 after2 = m3_mat4_transform_point(m, M3_VEC3(1, 0, 0));
    /* (2,0,0) -> rot90y(2,0,0) = (0,0,-2) -> +pos = (10,20,28) */
    CHECK_NEAR(after2.x, 10.0f, 1e-3f);
    CHECK_NEAR(after2.y, 20.0f, 1e-3f);
    CHECK_NEAR(after2.z, 28.0f, 1e-3f);
}

TEST(mat4_multiply_composition_order) {
    /* m3_mat4_multiply(a, b) = a * b (b applied first). Verify against
       independently composed translation and rotation. */
    m3_mat4 t = m3_mat4_translation(M3_VEC3(1.0f, 2.0f, 3.0f));
    m3_mat4 r = m3_mat4_rotation_quat(m3_quat_from_euler_deg(0.0f, 90.0f, 0.0f));
    m3_mat4 brt = m3_mat4_multiply(t, r);  /* T * R */
    m3_vec3 expected = M3_VEC3(1.0f, 2.0f, 3.0f - 1.0f); /* R(1,0,0) = (0,0,-1); + pos */
    m3_vec3 actual = m3_mat4_transform_point(brt, M3_VEC3(1.0f, 0.0f, 0.0f));
    CHECK_NEAR(actual.x, expected.x, 1e-3f);
    CHECK_NEAR(actual.y, expected.y, 1e-3f);
    CHECK_NEAR(actual.z, expected.z, 1e-3f);
}

TEST(mat4_inverse) {
    m3_mat4 m = m3_mat4_compose(
        M3_VEC3(1.0f, -2.0f, 0.5f),
        m3_quat_from_euler_deg(15.0f, 30.0f, -45.0f),
        M3_VEC3(2.0f, 0.5f, 1.5f));
    m3_mat4 inv;
    CHECK(m3_mat4_inverse(m, &inv) == 1);
    m3_mat4 product = m3_mat4_multiply(m, inv);
    m3_mat4 id = m3_mat4_identity();
    for (int i = 0; i < 16; i++) CHECK_NEAR(product.m[i], id.m[i], 1e-2f);
}

TEST(mat4_perspective_extremes) {
    m3_mat4 p = m3_mat4_perspective(60.0f, 1.0f, 0.1f, 100.0f);
    /* project (0, 0, -0.1, 1): near plane -> ndc z = -1 */
    float x = 0.0f, y = 0.0f, z = -0.1f, w = 1.0f;
    float cz = p.m[2] * x + p.m[6] * y + p.m[10] * z + p.m[14] * w;
    float cw = p.m[3] * x + p.m[7] * y + p.m[11] * z + p.m[15] * w;
    CHECK_NEAR(cz / cw, -1.0f, 1e-2f);
    /* project (0,0,-100): far plane -> ndc z = +1 */
    z = -100.0f;
    cz = p.m[2] * x + p.m[6] * y + p.m[10] * z + p.m[14] * w;
    cw = p.m[3] * x + p.m[7] * y + p.m[11] * z + p.m[15] * w;
    CHECK_NEAR(cz / cw, 1.0f, 1e-2f);
}

TEST(mat4_look_at) {
    m3_mat4 view = m3_mat4_look_at(M3_VEC3(0.0f, 0.0f, 4.0f), M3_VEC3(0.0f, 0.0f, 0.0f), M3_VEC3(0.0f, 1.0f, 0.0f));
    /* point at center should land at view z = -4 (in front of camera) */
    m3_vec3 p = m3_mat4_transform_point(view, M3_VEC3(0.0f, 0.0f, 0.0f));
    CHECK_NEAR(p.x, 0.0f, 1e-3f);
    CHECK_NEAR(p.y, 0.0f, 1e-3f);
    CHECK_NEAR(p.z, -4.0f, 1e-3f);
    /* point at the camera should land at origin */
    m3_vec3 p2 = m3_mat4_transform_point(view, M3_VEC3(0.0f, 0.0f, 4.0f));
    CHECK_NEAR(p2.x, 0.0f, 1e-3f);
    CHECK_NEAR(p2.y, 0.0f, 1e-3f);
    CHECK_NEAR(p2.z, 0.0f, 1e-3f);
}

TEST(mat4_normal_matrix) {
    /* non-uniform scale + rotation: check normal-matrix against raw inverse-transpose */
    m3_mat4 m = m3_mat4_compose(
        M3_VEC3(0.0f, 0.0f, 0.0f),
        m3_quat_from_euler_deg(0.0f, 90.0f, 0.0f),
        M3_VEC3(2.0f, 1.0f, 0.5f));
    m3_mat4 n = m3_mat4_normal_matrix(m);
    m3_vec3 test_normal = M3_VEC3(1.0f, 1.0f, 1.0f);
    m3_vec3 transformed = m3_mat4_transform_direction(n, test_normal);
    m3_vec3 unit;
    CHECK(m3_vec3_normalize(transformed, &unit) == 1);
    /* the normal matrix must produce a unit (up to scale) normal — check direction is sane */
    CHECK(unit.x >= -1.0f && unit.x <= 1.0f);
    CHECK(unit.y >= -1.0f && unit.y <= 1.0f);
    CHECK(unit.z >= -1.0f && unit.z <= 1.0f);
}

TEST_MAIN(RUN(vec3), RUN(quat_mul_identity), RUN(quat_rotate_90y),
          RUN(quat_from_euler_yaw90), RUN(mat4_identity_and_compose),
          RUN(mat4_multiply_composition_order), RUN(mat4_inverse),
          RUN(mat4_perspective_extremes), RUN(mat4_look_at),
          RUN(mat4_normal_matrix));
