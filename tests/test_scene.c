/*
 * test_scene.c - scene graph tests.
 */
#include "test.h"
#include "mini3/m3_scene.h"
#include <string.h>

static struct {
    size_t visited;
    size_t drawable;
} g_walk_ctx;

static void counting_visitor(m3_node* node, void* user) {
    (void)user;
    g_walk_ctx.visited++;
    if (m3_node_mesh(node)) g_walk_ctx.drawable++;
}

TEST(scene_lifecycle) {
    m3_scene* scene = m3_scene_create();
    CHECK(scene != NULL);
    CHECK(m3_scene_root(scene) != NULL);
    CHECK(strcmp(m3_node_name(m3_scene_root(scene)), "root") == 0);

    /* destroyed safely */
    m3_scene_destroy(scene);
}

TEST(scene_add_and_find) {
    m3_scene* scene = m3_scene_create();
    m3_node* a = m3_scene_add_node(scene, "cube");
    m3_node* b = m3_scene_add_node(scene, "light-marker");
    CHECK(a != NULL);
    CHECK(b != NULL);
    CHECK(strcmp(m3_node_name(a), "cube") == 0);
    CHECK(m3_scene_find(scene, "cube") == a);
    CHECK(m3_scene_find(scene, "nonexistent") == NULL);
    m3_scene_destroy(scene);
}

TEST(scene_hierarchy_world_matrix) {
    m3_scene* scene = m3_scene_create();
    /* parent: T(5,0,0) * rot(30deg yaw); child: T(0,2,0), 90deg yaw */
    m3_node* parent = m3_scene_add_node(scene, "parent");
    m3_node* child = m3_scene_add_node(scene, "child");
    m3_node* attached = m3_node_add_child(parent, child);
    CHECK(attached == child);
    CHECK(m3_node_child_count(parent) == 1);
    CHECK(m3_node_parent(child) == parent);

    m3_node_set_position(parent, M3_VEC3(5.0f, 0.0f, 0.0f));
    m3_node_set_rotation_euler(parent, 0.0f, 0.5235987755982989f /* 30 deg */, 0.0f);
    m3_node_set_position(child, M3_VEC3(0.0f, 2.0f, 0.0f));
    m3_node_set_rotation_euler(child, 0.0f, 1.5707963267948966f /* 90 deg */, 0.0f);

    m3_mat4 parent_world = m3_node_world(parent);
    m3_mat4 child_world = m3_node_world(child);

    /* expected child position: parent_world applied to (0,2,0) */
    m3_vec3 expected_pos = m3_mat4_transform_point(parent_world, M3_VEC3(0.0f, 2.0f, 0.0f));
    CHECK_NEAR(child_world.m[12], expected_pos.x, 1e-3f);
    CHECK_NEAR(child_world.m[13], expected_pos.y, 1e-3f);
    CHECK_NEAR(child_world.m[14], expected_pos.z, 1e-3f);

    /* expected child orientation: parent(30deg yaw) * child(90deg yaw) = 120deg yaw */
    m3_mat4 rot_parent = m3_mat4_rotation_quat(m3_quat_from_euler_deg(0.0f, 30.0f, 0.0f));
    m3_mat4 rot_child = m3_mat4_rotation_quat(m3_quat_from_euler_deg(0.0f, 90.0f, 0.0f));
    m3_mat4 expected_rot = m3_mat4_multiply(rot_parent, rot_child);
    for (int i = 0; i < 16; i++) {
        if (i == 15) continue; /* translation/scale check is already done via position */
        if (i >= 12) continue; /* row 3: w column, 0 except identity bottom-right */
        CHECK_NEAR(child_world.m[i], expected_rot.m[i], 2e-2f);
    }
    m3_scene_destroy(scene);
}

TEST(scene_walk_and_drawable) {
    m3_scene* scene = m3_scene_create();
    m3_mesh* mesh = m3_mesh_cube(1.0f);
    m3_material* material = m3_material_create(M3_VEC3(0.8f, 0.5f, 0.5f), M3_SHADING_FLAT);
    CHECK(mesh != NULL);
    CHECK(material != NULL);

    m3_node* a = m3_scene_add_node(scene, "a");
    m3_node_set_drawable(a, mesh, material);
    m3_node* b = m3_scene_add_node(scene, "b");
    m3_node* c = m3_scene_add_node(scene, "c");
    m3_node_set_drawable(c, mesh, material);

    CHECK(m3_node_mesh(a) == mesh);
    CHECK(m3_node_material(c) == material);
    CHECK(m3_node_mesh(b) == NULL);
    CHECK(m3_node_material(b) == NULL);

    g_walk_ctx.visited = 0;
    g_walk_ctx.drawable = 0;
    m3_scene_walk(scene, counting_visitor, NULL);
    /* root + a + b + c = 4 visits, 2 drawable */
    CHECK(g_walk_ctx.visited == 4);
    CHECK(g_walk_ctx.drawable == 2);

    m3_mesh_destroy(mesh);
    m3_material_destroy(material);
    m3_scene_destroy(scene);
}

TEST_MAIN(RUN(scene_lifecycle), RUN(scene_add_and_find), RUN(scene_hierarchy_world_matrix),
          RUN(scene_walk_and_drawable));
