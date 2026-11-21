/*
 * test_geometry_material.c - geometry + material + lighting tests.
 */
#include <string.h>
#include "test.h"
#include "mini3/m3_geometry.h"
#include "mini3/m3_material.h"
#include "mini3/m3_light.h"

TEST(cube_geometry) {
    m3_mesh* cube = m3_mesh_cube(1.0f);
    CHECK(cube != NULL);
    CHECK(m3_mesh_vertex_count(cube) == 24);
    CHECK(m3_mesh_index_count(cube) == 36);
    CHECK(m3_mesh_triangle_count(cube) == 12);
    CHECK(m3_mesh_positions(cube) != NULL);
    CHECK(m3_mesh_normals(cube) != NULL);
    CHECK(m3_mesh_colors(cube) != NULL);

    /* check a known vertex: +X face corners have x = 0.5 */
    const m3_vec3* pos = m3_mesh_positions(cube);
    CHECK(fabsf(pos[0].x - 0.5f) < 1e-4f);
    /* normals: +X face all have normal (1,0,0) */
    const m3_vec3* normals = m3_mesh_normals(cube);
    for (int i = 0; i < 4; i++) {
        CHECK(fabsf(normals[i].x - 1.0f) < 1e-4f);
        CHECK(fabsf(normals[i].y) < 1e-4f);
        CHECK(fabsf(normals[i].z) < 1e-4f);
    }
    m3_mesh_destroy(cube);
}

TEST(sphere_geometry) {
    m3_mesh* sphere = m3_mesh_sphere(1.0f, 8, 12);
    CHECK(sphere != NULL);
    CHECK(m3_mesh_vertex_count(sphere) == 9 * 12);
    CHECK(m3_mesh_triangle_count(sphere) == 8 * 12 * 2);
    /* every vertex should be at radius 1 */
    const m3_vec3* pos = m3_mesh_positions(sphere);
    for (size_t i = 0; i < m3_mesh_vertex_count(sphere); i++) {
        CHECK(fabsf(m3_vec3_len(pos[i]) - 1.0f) < 1e-3f);
    }
    /* normals are unit vectors */
    const m3_vec3* normals = m3_mesh_normals(sphere);
    for (size_t i = 0; i < m3_mesh_vertex_count(sphere); i++) {
        CHECK(fabsf(m3_vec3_len(normals[i]) - 1.0f) < 1e-3f);
    }
    m3_mesh_destroy(sphere);
}

TEST(plane_geometry) {
    m3_mesh* plane = m3_mesh_plane(2.0f, 1.0f);
    CHECK(plane != NULL);
    CHECK(m3_mesh_vertex_count(plane) == 4);
    CHECK(m3_mesh_triangle_count(plane) == 2);
    m3_mesh_destroy(plane);
}

TEST(mesh_create_validation) {
    m3_mesh_desc bad = {
        .positions = NULL, .normals = NULL, .colors = NULL, .indices = NULL,
        .vertex_count = 0, .index_count = 0,
    };
    CHECK(m3_mesh_create(&bad) == NULL);

    m3_vec3 pos = M3_VEC3(0, 0, 0);
    m3_mesh_desc good = {
        .positions = &pos, .normals = NULL, .colors = NULL,
        .indices = NULL, .vertex_count = 1, .index_count = 0,
    };
    CHECK(m3_mesh_create(&good) == NULL); /* needs at least 3 indices */

    uint32_t indices[6] = { 0, 0, 0, 0, 0, 0 };
    m3_mesh_desc good_desc = {
        .positions = (m3_vec3[]){ {0,0,0}, {1,0,0}, {0,1,0}, {1,1,0} },
        .normals = NULL, .colors = NULL,
        .indices = indices, .vertex_count = 4, .index_count = 6,
    };
    m3_mesh* mesh = m3_mesh_create(&good_desc);
    CHECK(mesh != NULL);
    CHECK(m3_mesh_vertex_count(mesh) == 4);
    /* indices are duplicated -> invalid under this mode, but the loader accepts them
       (it does not validate index range beyond these checks); destroy */
    m3_mesh_destroy(mesh);
}

TEST(material_lifecycle) {
    m3_material* material = m3_material_create(M3_VEC3(0.8f, 0.4f, 0.4f), M3_SHADING_FLAT);
    CHECK(material != NULL);
    CHECK(m3_material_shading(material) == M3_SHADING_FLAT);
    CHECK(m3_material_diffuse(material).x == 0.8f);

    m3_material_destroy(material);

    CHECK(m3_material_create(M3_VEC3(0, 0, 0), (m3_shading_mode)99) == NULL);

    CHECK(strcmp(m3_shading_mode_name(M3_SHADING_FLAT), "flat") == 0);
    CHECK(strcmp(m3_shading_mode_name(M3_SHADING_GOURAUD), "gouraud") == 0);
}

TEST(lighting_lifecycle) {
    m3_lighting lighting;
    m3_lighting_init(&lighting);
    CHECK(lighting.directional_count == 0);

    m3_lighting_set_ambient(&lighting, M3_VEC3(0.2f, 0.2f, 0.25f));
    CHECK(m3_vec3_dot(lighting.ambient, M3_VEC3(1, 1, 1)) == 0.65f);

    /* add a directional light; direction is normalized */
    m3_vec3 dir = M3_VEC3(10.0f, 1.0f, 5.0f);
    CHECK(m3_lighting_add_directional(&lighting, dir, M3_VEC3(1.0f, 1.0f, 0.9f)) == 0);
    CHECK(lighting.directional_count == 1);
    CHECK_NEAR(m3_vec3_len(lighting.directional[0].direction), 1.0f, 1e-4f);

    /* fill all slots */
    for (int i = 1; i < 4; i++) {
        CHECK(m3_lighting_add_directional(&lighting, M3_VEC3(0, 1, 0), M3_VEC3(0, 0, 0)) == 0);
    }
    CHECK(lighting.directional_count == 4);
    /* one more fails */
    CHECK(m3_lighting_add_directional(&lighting, M3_VEC3(0, 1, 0), M3_VEC3(0, 0, 0)) == -1);
    CHECK(lighting.directional_count == 4);
}

TEST_MAIN(RUN(cube_geometry), RUN(sphere_geometry), RUN(plane_geometry),
          RUN(mesh_create_validation), RUN(material_lifecycle), RUN(lighting_lifecycle));
