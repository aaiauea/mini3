/*
 * m3_camera.h - Mini3 cameras
 *
 * Perspective camera only (that is all the engine needs). The camera is a
 * position + orientation plus projection parameters; the matrices are computed
 * lazily per frame. There is no coupling to the scene graph beyond an
 * optional node transform — the camera stores its own transform.
 */
#ifndef M3_CAMERA_H
#define M3_CAMERA_H

#include "mini3/m3_math.h"

typedef struct m3_camera m3_camera;

m3_camera* m3_camera_create(float fov_y_deg, float aspect, float near, float far);
void       m3_camera_destroy(m3_camera* camera);

void  m3_camera_set_fov(m3_camera* camera, float fov_y_deg);
void  m3_camera_set_aspect(m3_camera* camera, float aspect);
void  m3_camera_set_near_far(m3_camera* camera, float near, float far);
void  m3_camera_set_position(m3_camera* camera, m3_vec3 position);
void  m3_camera_set_rotation(m3_camera* camera, m3_quat rotation);
/* Place the camera at `eye` so it looks at `target`. */
void  m3_camera_look_at(m3_camera* camera, m3_vec3 eye, m3_vec3 target, m3_vec3 up);

m3_vec3 m3_camera_position(const m3_camera* camera);
m3_quat m3_camera_rotation(const m3_camera* camera);
float   m3_camera_fov(const m3_camera* camera);
float   m3_camera_aspect(const m3_camera* camera);
float   m3_camera_near(const m3_camera* camera);
float   m3_camera_far(const m3_camera* camera);

/* Matrices. View is looking down -z with the camera-space +y as up. */
m3_mat4 m3_camera_view(const m3_camera* camera);
m3_mat4 m3_camera_projection(const m3_camera* camera);
m3_mat4 m3_camera_view_projection(const m3_camera* camera);

#endif /* M3_CAMERA_H */
