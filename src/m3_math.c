/*
 * m3_math.c - Mini3 math implementation (see m3_math.h for conventions).
 *
 * Storage convention: mat4.m[4*col + row] (column-major). A vector v transformed
 * by M gives out.x = m[0]*v.x + m[4]*v.y + m[8]*v.z (+m[12] for points).
 */
#include "mini3/m3_math.h"
#include <string.h>

/* ---------------- vec3 ---------------- */

m3_vec3 m3_vec3_add(m3_vec3 a, m3_vec3 b) { return M3_VEC3(a.x + b.x, a.y + b.y, a.z + b.z); }
m3_vec3 m3_vec3_sub(m3_vec3 a, m3_vec3 b) { return M3_VEC3(a.x - b.x, a.y - b.y, a.z - b.z); }
m3_vec3 m3_vec3_scale(m3_vec3 a, float s) { return M3_VEC3(a.x * s, a.y * s, a.z * s); }
float   m3_vec3_dot(m3_vec3 a, m3_vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
m3_vec3 m3_vec3_cross(m3_vec3 a, m3_vec3 b) {
    return M3_VEC3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
}
float m3_vec3_len(m3_vec3 a) { return sqrtf(a.x * a.x + a.y * a.y + a.z * a.z); }

int m3_vec3_normalize(m3_vec3 a, m3_vec3* out) {
    float len = m3_vec3_len(a);
    if (len < 1e-12f) {
        out->x = 0.0f; out->y = 0.0f; out->z = 0.0f;
        return 0;
    }
    out->x = a.x / len; out->y = a.y / len; out->z = a.z / len;
    return 1;
}

int m3_vec3_equals(m3_vec3 a, m3_vec3 b, float eps) {
    return fabsf(a.x - b.x) < eps && fabsf(a.y - b.y) < eps && fabsf(a.z - b.z) < eps;
}

/* ---------------- quaternion ---------------- */

m3_quat m3_quat_identity(void) { return M3_QUAT_IDENTITY; }

m3_quat m3_quat_mul(m3_quat a, m3_quat b) {
    /* a * b: b applied first. */
    return M3_QUAT(
        b.x * a.w + a.x * b.w + b.y * a.z - a.y * b.z,
        a.y * b.w + b.y * a.w + a.z * b.x - b.z * a.x,
        a.z * b.w + b.z * a.w + a.x * b.y - b.x * a.y,
        a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z);
}

m3_quat m3_quat_conj(m3_quat q) { return M3_QUAT(-q.x, -q.y, -q.z, q.w); }
m3_quat m3_quat_conjugate(m3_quat q) { return m3_quat_conj(q); }

m3_quat m3_quat_normalize(m3_quat q) {
    float len = sqrtf(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
    if (len < 1e-12f) return M3_QUAT(0.0f, 0.0f, 0.0f, 1.0f);
    return M3_QUAT(q.x / len, q.y / len, q.z / len, q.w / len);
}

float m3_quat_len(m3_quat q) {
    return sqrtf(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
}

m3_vec3 m3_quat_rotate_vec3(m3_quat q, m3_vec3 v) {
    /* v' = v + 2w(q x v) + 2(q x (q x v)), with q = (qv, w). */
    q = m3_quat_normalize(q);
    m3_vec3 qv = M3_VEC3(q.x, q.y, q.z);
    m3_vec3 t = m3_vec3_cross(qv, v);
    m3_vec3 t2 = m3_vec3_cross(qv, t);
    t = m3_vec3_scale(t, 2.0f * q.w);
    t2 = m3_vec3_scale(t2, 2.0f);
    return m3_vec3_add(v, m3_vec3_add(t, t2));
}

m3_quat m3_quat_from_euler(float pitch, float yaw, float roll) {
    /* q = yaw(Y) * pitch(X) * roll(Z). */
    float hy = sinf(yaw * 0.5f), sy = cosf(yaw * 0.5f);
    float hp = sinf(pitch * 0.5f), sp = cosf(pitch * 0.5f);
    float hr = sinf(roll * 0.5f), sr = cosf(roll * 0.5f);
    m3_quat qx = M3_QUAT(hp, 0.0f, 0.0f, sp); /* X: pitch */
    m3_quat qy = M3_QUAT(0.0f, hy, 0.0f, sy); /* Y: yaw */
    m3_quat qz = M3_QUAT(0.0f, 0.0f, hr, sr); /* Z: roll */
    return m3_quat_normalize(m3_quat_mul(m3_quat_mul(qy, qx), qz));
}

m3_quat m3_quat_from_euler_deg(float pitch_deg, float yaw_deg, float roll_deg) {
    return m3_quat_from_euler(
        pitch_deg * M3_PI / 180.0f, yaw_deg * M3_PI / 180.0f, roll_deg * M3_PI / 180.0f);
}

/* ---------------- mat4 ---------------- */

m3_mat4 m3_mat4_identity(void) {
    m3_mat4 id = {0};
    id.m[0] = 1.0f; id.m[5] = 1.0f; id.m[10] = 1.0f; id.m[15] = 1.0f;
    return id;
}

m3_mat4 m3_mat4_multiply(m3_mat4 a, m3_mat4 b) {
    m3_mat4 out;
    out.m[0]  = a.m[0]*b.m[0]  + a.m[4]*b.m[1]  + a.m[8]*b.m[2]  + a.m[12]*b.m[3];
    out.m[1]  = a.m[1]*b.m[0]  + a.m[5]*b.m[1]  + a.m[9]*b.m[2]  + a.m[13]*b.m[3];
    out.m[2]  = a.m[2]*b.m[0]  + a.m[6]*b.m[1]  + a.m[10]*b.m[2] + a.m[14]*b.m[3];
    out.m[3]  = a.m[3]*b.m[0]  + a.m[7]*b.m[1]  + a.m[11]*b.m[2] + a.m[15]*b.m[3];
    out.m[4]  = a.m[0]*b.m[4]  + a.m[4]*b.m[5]  + a.m[8]*b.m[6]  + a.m[12]*b.m[7];
    out.m[5]  = a.m[1]*b.m[4]  + a.m[5]*b.m[5]  + a.m[9]*b.m[6]  + a.m[13]*b.m[7];
    out.m[6]  = a.m[2]*b.m[4]  + a.m[6]*b.m[5]  + a.m[10]*b.m[6] + a.m[14]*b.m[7];
    out.m[7]  = a.m[3]*b.m[4]  + a.m[7]*b.m[5]  + a.m[11]*b.m[6] + a.m[15]*b.m[7];
    out.m[8]  = a.m[0]*b.m[8]  + a.m[4]*b.m[9]  + a.m[8]*b.m[10] + a.m[12]*b.m[11];
    out.m[9]  = a.m[1]*b.m[8]  + a.m[5]*b.m[9]  + a.m[9]*b.m[10] + a.m[13]*b.m[11];
    out.m[10] = a.m[2]*b.m[8]  + a.m[6]*b.m[9]  + a.m[10]*b.m[10] + a.m[14]*b.m[11];
    out.m[11] = a.m[3]*b.m[8]  + a.m[7]*b.m[9]  + a.m[11]*b.m[10] + a.m[15]*b.m[11];
    out.m[12] = a.m[0]*b.m[12] + a.m[4]*b.m[13] + a.m[8]*b.m[14] + a.m[12]*b.m[15];
    out.m[13] = a.m[1]*b.m[12] + a.m[5]*b.m[13] + a.m[9]*b.m[14] + a.m[13]*b.m[15];
    out.m[14] = a.m[2]*b.m[12] + a.m[6]*b.m[13] + a.m[10]*b.m[14] + a.m[14]*b.m[15];
    out.m[15] = a.m[3]*b.m[12] + a.m[7]*b.m[13] + a.m[11]*b.m[14] + a.m[15]*b.m[15];
    return out;
}

m3_mat4 m3_mat4_transpose(m3_mat4 a) {
    m3_mat4 out;
    out.m[0] = a.m[0];  out.m[1] = a.m[4];  out.m[2] = a.m[8];  out.m[3] = a.m[12];
    out.m[4] = a.m[1];  out.m[5] = a.m[5];  out.m[6] = a.m[9];  out.m[7] = a.m[13];
    out.m[8] = a.m[2];  out.m[9] = a.m[6];  out.m[10] = a.m[10]; out.m[11] = a.m[14];
    out.m[12] = a.m[3]; out.m[13] = a.m[7]; out.m[14] = a.m[11]; out.m[15] = a.m[15];
    return out;
}

m3_mat4 m3_mat4_translation(m3_vec3 v) {
    m3_mat4 out = m3_mat4_identity();
    out.m[12] = v.x; out.m[13] = v.y; out.m[14] = v.z;
    return out;
}

m3_mat4 m3_mat4_scale(m3_vec3 v) {
    m3_mat4 out = m3_mat4_identity();
    out.m[0] = v.x; out.m[5] = v.y; out.m[10] = v.z;
    return out;
}

m3_mat4 m3_mat4_rotation_quat(m3_quat q) {
    q = m3_quat_normalize(q);
    float x = q.x, y = q.y, z = q.z, w = q.w;
    m3_mat4 out = {0};
    out.m[0]  = 1.0f - 2.0f * (y * y + z * z);
    out.m[1]  = 2.0f * (x * y + w * z);
    out.m[2]  = 2.0f * (x * z - w * y);
    out.m[4]  = 2.0f * (x * y - w * z);
    out.m[5]  = 1.0f - 2.0f * (x * x + z * z);
    out.m[6]  = 2.0f * (y * z + w * x);
    out.m[8]  = 2.0f * (x * z + w * y);
    out.m[9]  = 2.0f * (y * z - w * x);
    out.m[10] = 1.0f - 2.0f * (x * x + y * y);
    out.m[15] = 1.0f;
    return out;
}

m3_mat4 m3_mat4_compose(m3_vec3 position, m3_quat rotation, m3_vec3 scale) {
    m3_mat4 t = m3_mat4_translation(position);
    m3_mat4 r = m3_mat4_rotation_quat(rotation);
    m3_mat4 s = m3_mat4_scale(scale);
    return m3_mat4_multiply(m3_mat4_multiply(t, r), s);
}

int m3_mat4_inverse(m3_mat4 in, m3_mat4* out) {
    /* Gauss-Jordan on augmented [a | I] with rows tracked in row-major storage. */
    float a[4][4], inv[4][4];
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            /* column-major in -> row-major a[r][c] = in.m[4*c + r] */
            a[r][c] = in.m[4 * c + r];
            inv[r][c] = (r == c) ? 1.0f : 0.0f;
        }
    }
    for (int i = 0; i < 4; i++) {
        /* partial pivot */
        int pivot = i;
        float max = fabsf(a[i][i]);
        for (int r = i + 1; r < 4; r++) {
            if (fabsf(a[r][i]) > max) { max = fabsf(a[r][i]); pivot = r; }
        }
        if (max < 1e-12f) return 0;
        if (pivot != i) {
            for (int j = 0; j < 4; j++) {
                float tmp = a[i][j]; a[i][j] = a[pivot][j]; a[pivot][j] = tmp;
                tmp = inv[i][j]; inv[i][j] = inv[pivot][j]; inv[pivot][j] = tmp;
            }
        }
        /* normalize pivot row */
        float piv = a[i][i];
        for (int j = 0; j < 4; j++) { a[i][j] /= piv; inv[i][j] /= piv; }
        /* eliminate all other rows */
        for (int r = 0; r < 4; r++) {
            if (r == i) continue;
            float factor = a[r][i];
            if (fabsf(factor) < 1e-12f) continue;
            for (int j = 0; j < 4; j++) {
                a[r][j] -= factor * a[i][j];
                inv[r][j] -= factor * inv[i][j];
            }
        }
    }
    /* transpose back to column-major */
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            out->m[4 * c + r] = inv[r][c];
        }
    }
    return 1;
}

m3_vec3 m3_mat4_transform_point(m3_mat4 m, m3_vec3 p) {
    float x = m.m[0] * p.x + m.m[4] * p.y + m.m[8] * p.z + m.m[12];
    float y = m.m[1] * p.x + m.m[5] * p.y + m.m[9] * p.z + m.m[13];
    float z = m.m[2] * p.x + m.m[6] * p.y + m.m[10] * p.z + m.m[14];
    return M3_VEC3(x, y, z);
}

m3_vec3 m3_mat4_transform_direction(m3_mat4 m, m3_vec3 v) {
    float x = m.m[0] * v.x + m.m[4] * v.y + m.m[8] * v.z;
    float y = m.m[1] * v.x + m.m[5] * v.y + m.m[9] * v.z;
    float z = m.m[2] * v.x + m.m[6] * v.y + m.m[10] * v.z;
    return M3_VEC3(x, y, z);
}

m3_mat4 m3_mat4_normal_matrix(m3_mat4 m) {
    /* N = transpose(inverse(linear 3x3 L)). Row-major linear part:
       L[0] = (m[0], m[4], m[8]), L[1] = (m[1], m[5], m[9]), L[2] = (m[2], m[6], m[10]). */
    float m00 = m.m[0], m01 = m.m[4], m02 = m.m[8];
    float m10 = m.m[1], m11 = m.m[5], m12 = m.m[9];
    float m20 = m.m[2], m21 = m.m[6], m22 = m.m[10];
    float det = m00 * (m11 * m22 - m12 * m21) - m01 * (m10 * m22 - m12 * m20) + m02 * (m10 * m21 - m11 * m20);
    if (fabsf(det) < 1e-12f) return m3_mat4_identity();
    float ia00 = m11 * m22 - m12 * m21;
    float ia01 = m02 * m21 - m01 * m22;
    float ia02 = m01 * m12 - m02 * m11;
    float ia10 = m12 * m20 - m10 * m22;
    float ia11 = m00 * m22 - m02 * m20;
    float ia12 = m02 * m10 - m00 * m12;
    float ia20 = m10 * m21 - m11 * m20;
    float ia21 = m01 * m20 - m00 * m21;
    float ia22 = m00 * m11 - m01 * m10;
    float inv_det = 1.0f / det;
    /* Normal matrix N = (L^-1)^T. Column-major store: N[row][col] at out.m[4*col+row].
       N[i][j] = inv[j][i], so:
         col0 row0 = N[0][0] = inv[0][0] = ia00
         col0 row1 = N[1][0] = inv[0][1] = ia01
         col0 row2 = N[2][0] = inv[0][2] = ia02
         col1 row0 = N[0][1] = inv[1][0] = ia10
         col1 row1 = N[1][1] = inv[1][1] = ia11
         col1 row2 = N[2][1] = inv[1][2] = ia12
         col2 row0 = N[0][2] = inv[2][0] = ia20
         col2 row1 = N[1][2] = inv[2][1] = ia21
         col2 row2 = N[2][2] = inv[2][2] = ia22 */
    m3_mat4 out = m3_mat4_identity();
    out.m[0] = ia00 * inv_det;
    out.m[1] = ia01 * inv_det;
    out.m[2] = ia02 * inv_det;
    out.m[4] = ia10 * inv_det;
    out.m[5] = ia11 * inv_det;
    out.m[6] = ia12 * inv_det;
    out.m[8] = ia20 * inv_det;
    out.m[9] = ia21 * inv_det;
    out.m[10] = ia22 * inv_det;
    return out;
}

m3_mat4 m3_mat4_perspective(float fov_y_deg, float aspect, float near, float far) {
    float f = 1.0f / tanf((fov_y_deg * M3_PI / 180.0f) * 0.5f);
    m3_mat4 out = {0};
    out.m[0] = f / aspect;
    out.m[5] = f;
    out.m[10] = (far + near) / (near - far);
    out.m[11] = -1.0f;
    out.m[14] = 2.0f * far * near / (near - far);
    return out;
}

m3_mat4 m3_mat4_look_at(m3_vec3 eye, m3_vec3 center, m3_vec3 up) {
    m3_vec3 f;
    if (!m3_vec3_normalize(m3_vec3_sub(center, eye), &f)) {
        f = M3_VEC3(0.0f, 0.0f, -1.0f);
    }
    m3_vec3 r;
    if (!m3_vec3_normalize(m3_vec3_cross(f, up), &r)) {
        r = M3_VEC3(1.0f, 0.0f, 0.0f);
    }
    m3_vec3 u = m3_vec3_cross(r, f);
    /* view = R^T * translation(-eye). Storage:
       row 0 of the 3x3 part is the camera-right axis, row 1 the camera-up axis,
       row 2 the camera z-axis (-f). Row-major to column-major storage is
       m[4*col + row] = M[row][col]:
         m[0] = r.x;  m[1] = u.x;  m[2] = -f.x;
         m[4] = r.y;  m[5] = u.y;  m[6] = -f.y;
         m[8] = r.z;  m[9] = u.z;  m[10] = -f.z;
       and translation m[12] = -dot(r, eye); m[13] = -dot(u, eye); m[14] = dot(f, eye). */
    m3_mat4 out = {0};
    out.m[0]  = r.x;  out.m[1]  = u.x;  out.m[2]  = -f.x;
    out.m[4]  = r.y;  out.m[5]  = u.y;  out.m[6]  = -f.y;
    out.m[8]  = r.z;  out.m[9]  = u.z;  out.m[10] = -f.z;
    out.m[12] = -m3_vec3_dot(r, eye);
    out.m[13] = -m3_vec3_dot(u, eye);
    out.m[14] = m3_vec3_dot(f, eye);
    out.m[15] = 1.0f;
    return out;
}

m3_vec3 m3_mat4_translation_of(m3_mat4 m) { return M3_VEC3(m.m[12], m.m[13], m.m[14]); }
