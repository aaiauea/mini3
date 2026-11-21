/*
 * m3_scene.c - Mini3 scene graph implementation.
 *
 * Nodes are plain malloc'd structs. `m3_node_world` computes the world matrix
 * on demand by walking up the parent chain: world = parent_world * local.
 * No cached state, so no stale matrices.
 */
#include "mini3/m3_scene.h"
#include <stdlib.h>
#include <string.h>

#define M3_CHILD_INIT 2
#define M3_CHILD_GROW 4

struct m3_node {
    char* name;
    m3_node* parent;
    m3_node** children;
    size_t child_count;
    size_t child_capacity;
    m3_vec3 position;
    m3_quat rotation;
    m3_vec3 scale;
    m3_mesh* mesh;
    m3_material* material;
};

struct m3_scene {
    m3_node* root;
    size_t node_count;      /* total allocated nodes */
    size_t nodes_capacity;
    m3_node** nodes;        /* flat list, root included */
};

/* ---- internal helpers ---- */

static char* m3_strdup(const char* s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char* copy = malloc(n);
    if (!copy) return NULL;
    memcpy(copy, s, n);
    return copy;
}

static m3_node* m3_node_create_raw(m3_scene* scene) {
    if (!scene) return NULL;
    if (scene->node_count >= scene->nodes_capacity) {
        size_t new_cap = scene->nodes_capacity == 0 ? 8 : scene->nodes_capacity * 2;
        m3_node** new_nodes = realloc(scene->nodes, sizeof(m3_node*) * new_cap);
        if (!new_nodes) return NULL;
        scene->nodes = new_nodes;
        scene->nodes_capacity = new_cap;
    }
    m3_node* node = calloc(1, sizeof(m3_node));
    if (!node) return NULL;
    node->name = NULL;
    node->parent = NULL;
    node->children = NULL;
    node->child_count = 0;
    node->child_capacity = 0;
    node->position = M3_VEC3(0.0f, 0.0f, 0.0f);
    node->rotation = m3_quat_identity();
    node->scale = M3_VEC3(1.0f, 1.0f, 1.0f);
    node->mesh = NULL;
    node->material = NULL;
    scene->nodes[scene->node_count++] = node;
    return node;
}

static int m3_node_add_child_raw(m3_node* parent, m3_node* child) {
    if (!parent || !child) return -1;
    if (parent->child_count >= parent->child_capacity) {
        size_t new_cap = parent->child_capacity == 0 ? M3_CHILD_INIT : parent->child_capacity + M3_CHILD_GROW;
        m3_node** new_children = realloc(parent->children, sizeof(m3_node*) * new_cap);
        if (!new_children) return -1;
        parent->children = new_children;
        parent->child_capacity = new_cap;
    }
    parent->children[parent->child_count++] = child;
    child->parent = parent;
    return 0;
}

/* ---- scene lifecycle ---- */

m3_scene* m3_scene_create(void) {
    m3_scene* scene = calloc(1, sizeof(m3_scene));
    if (!scene) return NULL;
    m3_node* root = m3_node_create_raw(scene);
    if (!root) { free(scene); return NULL; }
    root->name = m3_strdup("root");
    scene->root = root;
    return scene;
}

void m3_scene_destroy(m3_scene* scene) {
    if (!scene) return;
    for (size_t i = 0; i < scene->node_count; i++) {
        m3_node* n = scene->nodes[i];
        free(n->name);
        free(n->children);
        free(n);
    }
    free(scene->nodes);
    free(scene);
}

/* ---- nodes ---- */

m3_node* m3_scene_add_node(m3_scene* scene, const char* name) {
    if (!scene) return NULL;
    m3_node* node = m3_node_create_raw(scene);
    if (!node) return NULL;
    node->name = m3_strdup(name);
    if (m3_node_add_child_raw(scene->root, node) != 0) {
        /* detach; caller owns cleanup */
        free(node->name);
        free(node);
        /* remove from flat list (shift) */
        for (size_t i = scene->node_count - 1; i > 0; i--) scene->nodes[i] = scene->nodes[i - 1];
        scene->node_count--;
        return NULL;
    }
    return node;
}

m3_node* m3_scene_find(m3_scene* scene, const char* name) {
    if (!scene || !name) return NULL;
    for (size_t i = 0; i < scene->node_count; i++) {
        if (scene->nodes[i]->name && strcmp(scene->nodes[i]->name, name) == 0)
            return scene->nodes[i];
    }
    return NULL;
}

m3_node* m3_scene_root(m3_scene* scene) { return scene ? scene->root : NULL; }
const char* m3_node_name(const m3_node* node) { return (node && node->name) ? node->name : ""; }
m3_node* m3_node_parent(const m3_node* node) { return node ? node->parent : NULL; }
size_t m3_node_child_count(const m3_node* node) { return (node && node->children) ? node->child_count : 0; }
m3_node* m3_node_child_at(const m3_node* node, size_t index) {
    if (!node || index >= node->child_count) return NULL;
    return node->children[index];
}

m3_node* m3_node_add_child(m3_node* parent, m3_node* child) {
    if (!parent || !child) return NULL;
    /* detach from previous parent if any */
    m3_node* old_parent = child->parent;
    if (old_parent) {
        for (size_t i = 0; i < old_parent->child_count; i++) {
            if (old_parent->children[i] == child) {
                for (size_t j = i + 1; j < old_parent->child_count; j++) {
                    old_parent->children[j - 1] = old_parent->children[j];
                }
                old_parent->child_count--;
                break;
            }
        }
    }
    child->parent = NULL;
    if (m3_node_add_child_raw(parent, child) != 0) return NULL;
    return child;
}

/* ---- transforms ---- */

void m3_node_set_position(m3_node* node, m3_vec3 position) {
    if (node) node->position = position;
}
void m3_node_set_rotation(m3_node* node, m3_quat rotation) {
    if (node) node->rotation = m3_quat_normalize(rotation);
}
void m3_node_set_scale(m3_node* node, m3_vec3 scale) {
    if (node) node->scale = scale;
}
void m3_node_set_rotation_euler(m3_node* node, float pitch, float yaw, float roll) {
    if (node) node->rotation = m3_quat_from_euler(pitch, yaw, roll);
}
void m3_node_set_transform(m3_node* node, m3_vec3 position, m3_quat rotation, m3_vec3 scale) {
    if (!node) return;
    node->position = position;
    node->rotation = m3_quat_normalize(rotation);
    node->scale = scale;
}
void m3_node_get_transform(const m3_node* node, m3_vec3* position, m3_quat* rotation, m3_vec3* scale) {
    if (!node) return;
    if (position) *position = node->position;
    if (rotation) *rotation = node->rotation;
    if (scale) *scale = node->scale;
}

m3_mat4 m3_node_local(const m3_node* node) {
    if (!node) return m3_mat4_identity();
    return m3_mat4_compose(node->position, node->rotation, node->scale);
}

m3_mat4 m3_node_world(const m3_node* node) {
    if (!node) return m3_mat4_identity();
    m3_mat4 local = m3_node_local(node);
    if (!node->parent) return local;
    return m3_mat4_multiply(m3_node_world(node->parent), local);
}

/* ---- drawable attachment ---- */

void m3_node_set_drawable(m3_node* node, m3_mesh* mesh, m3_material* material) {
    if (!node) return;
    node->mesh = mesh;
    node->material = material;
}
m3_mesh* m3_node_mesh(const m3_node* node) { return node ? node->mesh : NULL; }
m3_material* m3_node_material(const m3_node* node) { return node ? node->material : NULL; }

/* ---- traversal ---- */

static void m3_walk_recursive(m3_node* node, m3_node_visitor visitor, void* user) {
    if (!node) return;
    /* children list is stable during this call because callers must not mutate
       the tree asynchronously */
    visitor(node, user);
    for (size_t i = 0; i < node->child_count; i++) {
        m3_walk_recursive(node->children[i], visitor, user);
    }
}

void m3_scene_walk(m3_scene* scene, m3_node_visitor visitor, void* user) {
    if (!scene || !visitor) return;
    m3_walk_recursive(scene->root, visitor, user);
}
