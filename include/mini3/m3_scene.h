/*
 * m3_scene.h - Mini3 scene graph
 *
 * A minimal hierarchical node tree. Each node carries a local TRS transform and
 * an optional drawable (mesh + material). World matrices are computed lazily and
 * accurately at call time -- there is no explicit update step and no stale
 * cache. Nodes are created through the scene.
 *
 * Thread-safety: not thread-safe. Configure single-threaded use.
 */
#ifndef M3_SCENE_H
#define M3_SCENE_H

#include "mini3/m3_math.h"
#include "mini3/m3_geometry.h"
#include "mini3/m3_material.h"
#include <stddef.h>
#include <stdint.h>

typedef struct m3_node m3_node;
typedef struct m3_scene m3_scene;

/* ---- scene lifecycle ---- */
m3_scene* m3_scene_create(void);
void      m3_scene_destroy(m3_scene* scene);

/* ---- nodes ---- */
/* Creates a node attached to the scene root. `name` is copied. */
m3_node* m3_scene_add_node(m3_scene* scene, const char* name);
/* Finds a node by name (first match, may return NULL). */
m3_node* m3_scene_find(m3_scene* scene, const char* name);

/* Read-only accessors */
m3_node* m3_scene_root(m3_scene* scene);
const char* m3_node_name(const m3_node* node);
m3_node* m3_node_parent(const m3_node* node);
size_t   m3_node_child_count(const m3_node* node);
m3_node* m3_node_child_at(const m3_node* node, size_t index);

/* Attach an existing node as a child of `parent`. The child is detached from
 * its previous parent (if any) first. ~ Parent and child must be from the
 * same scene. */
m3_node* m3_node_add_child(m3_node* parent, m3_node* child);

/* ---- transforms ---- */
void    m3_node_set_position(m3_node* node, m3_vec3 position);
void    m3_node_set_rotation(m3_node* node, m3_quat rotation);
void    m3_node_set_scale(m3_node* node, m3_vec3 scale);
void    m3_node_set_rotation_euler(m3_node* node, float pitch, float yaw, float roll);
void    m3_node_set_transform(m3_node* node, m3_vec3 position, m3_quat rotation, m3_vec3 scale);
void    m3_node_get_transform(const m3_node* node, m3_vec3* position, m3_quat* rotation, m3_vec3* scale);

/* Local matrix: T * R * S. */
m3_mat4 m3_node_local(const m3_node* node);
/* World matrix: parent_world * local. Walks the tree; safe for any depth. */
m3_mat4 m3_node_world(const m3_node* node);

/* ---- drawable attachment ---- */
/* Attach a mesh + material to a node. The scene does NOT take ownership of
 * either -- the caller owns them and keeps them alive while the node exists. */
void          m3_node_set_drawable(m3_node* node, m3_mesh* mesh, m3_material* material);
m3_mesh*      m3_node_mesh(const m3_node* node);
m3_material*  m3_node_material(const m3_node* node);

/* ---- traversal ---- */
typedef void (*m3_node_visitor)(m3_node* node, void* user);
/* Calls visitor(root), then each descendant depth-first in creation order. */
void m3_scene_walk(m3_scene* scene, m3_node_visitor visitor, void* user);

#endif /* M3_SCENE_H */
