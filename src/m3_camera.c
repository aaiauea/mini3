/*
 * m3_camera.c - Mini3 perspective camera implementation.
 *
 * View matrix = inverse(T * R) where R is the camera's world rotation.
 * Because the camera transform is pure translation + rotation (no scale),
 * inverse(T*R) = R^T (recenter): view = R^T * translation(-position).
 */
#include "mini3/m3_camera.h"
#include <stdlib.h>
#include <math.h>

struct m3_camera {
    float fov_y_deg;
    float aspect;
    float near;
    float far;
    m3_vec3 position;
    m3_quat rotation;
};

m3_camera* m3_camera_create(float fov_y_deg, float aspect, float near, float far) {
    m3_camera* camera = malloc(sizeof(m3_camera));
    if (!camera) return NULL;
    camera->fov_y_deg = fov_y_deg;
    camera->aspect = aspect;
    camera->near = near;
    camera->far = far;
    camera->position = M3_VEC3(0.0f, 0.0f, 1.0f);
    camera->rotation = m3_quat_identity();
    return camera;
}

void m3_camera_destroy(m3_camera* camera) { free(camera); }

void m3_camera_set_fov(m3_camera* camera, float fov_y_deg) {
    if (camera) camera->fov_y_deg = fov_y_deg;
}
void m3_camera_set_aspect(m3_camera* camera, float aspect) {
    if (camera) camera->aspect = aspect;
}
void m3_camera_set_near_far(m3_camera* camera, float near, float far) {
    if (camera) { camera->near = near; camera->far = far; }
}
void m3_camera_set_position(m3_camera* camera, m3_vec3 position) {
    if (camera) camera->position = position;
}
void m3_camera_set_rotation(m3_camera* camera, m3_quat rotation) {
    if (camera) camera->rotation = m3_quat_normalize(rotation);
}

m3_vec3 m3_camera_position(const m3_camera* camera) {
    return camera ? camera->position : M3_VEC3(0, 0, 0);
}
m3_quat m3_camera_rotation(const m3_camera* camera) {
    return camera ? camera->rotation : m3_quat_identity();
}
float m3_camera_fov(const m3_camera* camera) { return camera ? camera->fov_y_deg : 0.0f; }
float m3_camera_aspect(const m3_camera* camera) { return camera ? camera->aspect : 1.0f; }
float m3_camera_near(const m3_camera* camera) { return camera ? camera->near : 0.1f; }
float m3_camera_far(const m3_camera* camera) { return camera ? camera->far : 100.0f; }

m3_mat4 m3_camera_view(const m3_camera* camera) {
    if (!camera) return m3_mat4_identity();
    /* world = T * R; view = inverse(world). For pure T*R:
       view_rotation = R^T ; view_translation = -(R^T * position). */
    m3_mat4 r = m3_mat4_rotation_quat(camera->rotation);
    m3_mat4 view = m3_mat4_transpose(r);
    /* view.m[12..14] = -view_rotation * position */
    m3_vec3 p = camera->position;
    float tx = view.m[0] * p.x + view.m[4] * p.y + view.m[8] * p.z;
    float ty = view.m[1] * p.x + view.m[5] * p.y + view.m[9] * p.z;
    float tz = view.m[2] * p.x + view.m[6] * p.y + view.m[10] * p.z;
    view.m[12] = -tx; view.m[13] = -ty; view.m[14] = -tz; view.m[15] = 1.0f;
    return view;
}

void m3_camera_look_at(m3_camera* camera, m3_vec3 eye, m3_vec3 target, m3_vec3 up) {
    if (!camera) return;
    m3_mat4 view = m3_mat4_look_at(eye, target, up);
    /* world rotation = transpose of the view's rotation part.
       Column-major view: view.m[4*c+r] is the (row r, col c) of the view matrix.
       The world rotation R satisfies (R^T) = view_rotation. */
    float rot[16] = {0};
    for (int r = 0; r < 3; r++)
        for (int c = 0; c < 3; c++)
            rot[4 * c + r] = view.m[4 * r + c];
    rot[15] = 1.0f;

    float m00 = rot[0], m01 = rot[4], m02 = rot[8];
    float m10 = rot[1], m11 = rot[5], m12 = rot[9];
    float m20 = rot[2], m21 = rot[6], m22 = rot[10];
    float trace = m00 + m11 + m22;
    float w, x, y, z;
    if (trace > 0.0f) {
        float s = sqrtf(trace + 1.0f) * 2.0f;
        w = 0.25f * s;
        x = (m21 - m12) / s;
        y = (m02 - m20) / s;
        z = (m10 - m01) / s;
    } else if (m00 > m11 && m00 > m22) {
        float s = sqrtf(1.0f + m00 - m11 - m22) * 2.0f;
        w = (m21 - m12) / s;
        x = 0.25f * s;
        y = (m01 + m10) / s;
        z = (m02 + m20) / s;
    } else if (m11 > m22) {
        float s = sqrtf(1.0f + m11 - m00 - m22) * 2.0f;
        w = (m02 - m20) / s;
        x = (m01 + m10) / s;
        y = 0.25f * s;
        z = (m12 + m21) / s;
    } else {
        float s = sqrtf(1.0f + m22 - m00 - m11) * 2.0f;
        w = (m10 - m01) / s;
        x = (m02 + m20) / s;
        y = (m12 + m21) / s;
        z = 0.25f * s;
    }
    camera->position = eye;
    camera->rotation = m3_quat_normalize(M3_QUAT(x, y, z, w));
}

m3_mat4 m3_camera_projection(const m3_camera* camera) {
    if (!camera) return m3_mat4_identity();
    return m3_mat4_perspective(camera->fov_y_deg, camera->aspect, camera->near, camera->far);
}

m3_mat4 m3_camera_view_projection(const m3_camera* camera) {
    return m3_mat4_multiply(m3_camera_projection(camera), m3_camera_view(camera));
}
