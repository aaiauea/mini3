/*
 * m3_math.h - Mini3 mathematical primitives
 *
 * Lightweight, dependency-free vector/quaternion/4x4-matrix math.
 * Conventions:
 *   - Right-handed coordinate system.
 *   - vec3 positions: x (right), y (up), z (toward viewer / toward camera).
 *   - mat4 is stored column-major: element (row i, col j) lives at m3_mat4.m[4*j + i].
 *   - m3_mat4_multiply(a, b) == a * b  (the right-most matrix is applied first).
 *   - Point transforms are affine: p' = M * (x, y, z, 1).
 *
 * Only the subset of math the engine needs is provided; nothing here knows
 * anything about cameras, scenes, or rendering backends.
 */
#ifndef M3_MATH_H
#define M3_MATH_H

#include <math.h>
#include <stddef.h>
#include <stdint.h>

#define M3_PI 3.14159265358979323846f

/* ---------------- vec3 ---------------- */
typedef struct {
    float x, y, z;
} m3_vec3;

#define M3_VEC3(x, y, z) ((m3_vec3){(x), (y), (z)})

m3_vec3 m3_vec3_add(m3_vec3 a, m3_vec3 b);
m3_vec3 m3_vec3_sub(m3_vec3 a, m3_vec3 b);
m3_vec3 m3_vec3_scale(m3_vec3 a, float s);
float   m3_vec3_dot(m3_vec3 a, m3_vec3 b);
m3_vec3 m3_vec3_cross(m3_vec3 a, m3_vec3 b);
float   m3_vec3_len(m3_vec3 a);
/* Returns false (and writes zero) if a has (near-)zero length. */
int     m3_vec3_normalize(m3_vec3 a, m3_vec3* out);
int     m3_vec3_equals(m3_vec3 a, m3_vec3 b, float eps);

/* ---------------- quaternion ----------------
 * Hamilton product convention. q = (x, y, z, w).
 * m3_quat_mul(a, b) = a * b  (b applied first, matching the mat4 convention).
 */
typedef struct {
    float x, y, z, w;
} m3_quat;

#define M3_QUAT_IDENTITY ((m3_quat){0.0f, 0.0f, 0.0f, 1.0f})
#define M3_QUAT(x, y, z, w) ((m3_quat){(x), (y), (z), (w)})

m3_quat m3_quat_identity(void);
m3_quat m3_quat_mul(m3_quat a, m3_quat b);
m3_quat m3_quat_conj(m3_quat q);
m3_quat m3_quat_conjugate(m3_quat q);
m3_quat m3_quat_normalize(m3_quat q);
float   m3_quat_len(m3_quat q);
/* q * v * q^-1 in right-handed space. */
m3_vec3 m3_quat_rotate_vec3(m3_quat q, m3_vec3 v);
/* Pitch (X), yaw (Y), roll (Z) in radians, applied yaw * pitch * roll. */
m3_quat m3_quat_from_euler(float pitch, float yaw, float roll);
/* Rotation from an arc-length of yaw/pitch/roll given in degrees. */
m3_quat m3_quat_from_euler_deg(float pitch_deg, float yaw_deg, float roll_deg);

/* ---------------- 4x4 matrix ---------------- */
typedef struct {
    float m[16]; /* column-major; row i col j at m[4*j + i] */
} m3_mat4;

m3_mat4 m3_mat4_identity(void);
m3_mat4 m3_mat4_multiply(m3_mat4 a, m3_mat4 b);
m3_mat4 m3_mat4_transpose(m3_mat4 a);
m3_mat4 m3_mat4_translation(m3_vec3 v);
m3_mat4 m3_mat4_scale(m3_vec3 v);
m3_mat4 m3_mat4_rotation_quat(m3_quat q);
m3_mat4 m3_mat4_compose(m3_vec3 position, m3_quat rotation, m3_vec3 scale);
/* Inverse of the full 4x4. Returns false if the matrix is singular. */
int     m3_mat4_inverse(m3_mat4 m, m3_mat4* out);
/* M * (p, 1). */
m3_vec3 m3_mat4_transform_point(m3_mat4 m, m3_vec3 p);
/* M * (v, 0) -- direction / normal, no translation. */
m3_vec3 m3_mat4_transform_direction(m3_mat4 m, m3_vec3 v);
/* World-space 3x3 rotation part (columns, normalized rows not guaranteed). */
m3_mat4 m3_mat4_normal_matrix(m3_mat4 m);
/* OpenGL-style perspective projection matrix, right-handed.
 * fov in degrees. Near plane maps to ndc z = -1, far to z = +1. */
m3_mat4 m3_mat4_perspective(float fov_y_deg, float aspect, float near, float far);
/* Right-handed look-at view matrix looking down -z (OpenGL-style). */
m3_mat4 m3_mat4_look_at(m3_vec3 eye, m3_vec3 center, m3_vec3 up);
/* Translation component of the matrix (m[12], m[13], m[14]). */
m3_vec3 m3_mat4_translation_of(m3_mat4 m);

#endif /* M3_MATH_H */
