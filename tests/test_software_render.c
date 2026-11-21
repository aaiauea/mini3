/*
 * test_software_render.c - end-to-end software renderer tests.
 *
 * Renders the rotating, lit cube through the FULL mini3 pipeline:
 * scene -> frame -> software backend -> framebuffer -> checks.
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "test.h"
#include "mini3/m3_renderer.h"
#include "../backends/software/m3_software.h"

#define REF_W 96
#define REF_H 96

/* Builds the rotating-lit-cube scene and renders one frame.
   The caller owns the returned backend (and must destroy it later).
   Captures: the rotation angle and the shared scene/camera/z for transparency. */
static m3_software* render_cube(float rotation_deg, m3_shading_mode mode) {
    const m3_renderer_interface* renderer = m3_software_renderer_get_interface();

    m3_scene* scene = m3_scene_create();
    m3_mesh* mesh = m3_mesh_cube(1.0f);
    m3_material* material = m3_material_create(M3_VEC3(0.85f, 0.55f, 0.45f), mode);
    m3_node* cube = m3_scene_add_node(scene, "cube");
    m3_node_set_drawable(cube, mesh, material);
    m3_node_set_rotation_euler(cube, 0.0f, rotation_deg * M3_PI / 180.0f, 0.0f);

    m3_camera* camera = m3_camera_create(45.0f, 1.0f, 0.1f, 100.0f);
    m3_camera_look_at(camera, M3_VEC3(0.0f, 1.4f, 4.0f), M3_VEC3(0.0f, 0.0f, 0.0f), M3_VEC3(0.0f, 1.0f, 0.0f));

    m3_lighting lights;
    m3_lighting_init(&lights);
    m3_lighting_set_ambient(&lights, M3_VEC3(0.22f, 0.22f, 0.3f));
    m3_lighting_add_directional(&lights, M3_VEC3(0.4f, 1.0f, 0.6f), M3_VEC3(1.0f, 0.95f, 0.9f));

    m3_frame frame;
    m3_frame_build(&frame, scene, camera, &lights);

    m3_software* backend = (m3_software*)renderer->create(NULL, NULL);
    renderer->set_viewport(backend, 0, 0, REF_W, REF_H);
    void* geometry = renderer->create_geometry(backend, mesh);
    void* shader = renderer->create_shader(backend, mode);
    renderer->begin_frame(backend, &frame);
    for (size_t i = 0; i < frame.draw_count; i++) {
        renderer->submit(backend, &frame, &frame.draws[i], geometry, shader);
    }
    renderer->end_frame(backend, &frame);

    m3_frame_free(&frame);
    renderer->destroy_geometry(backend, geometry);
    renderer->destroy_shader(backend, shader);
    /* caller owns the backend */
    m3_material_destroy(material);
    m3_mesh_destroy(mesh);
    m3_camera_destroy(camera);
    m3_scene_destroy(scene);
    (void)frame;
    return backend;
}

/* Destroy any backend helper state. The software backend owns the framebuffer. */
static void destroy_backend(m3_software* backend) {
    const m3_renderer_interface* renderer = m3_software_renderer_get_interface();
    /* resources were already freed; just free the backend */
    renderer->destroy(backend);
}

static float pixel_brightness(m3_software* backend, int x, int y) {
    float rgb[3] = { 0, 0, 0 };
    m3_software_get_pixel(backend, x, y, rgb);
    return (rgb[0] + rgb[1] + rgb[2]) / 3.0f;
}

static size_t differing_pixels(m3_software* a, m3_software* b) {
    int w, h;
    m3_software_viewport_size(a, &w, &h);
    size_t diff = 0;
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            float ra[3], rb[3];
            m3_software_get_pixel(a, x, y, ra);
            m3_software_get_pixel(b, x, y, rb);
            float d = fabsf(ra[0] - rb[0]) + fabsf(ra[1] - rb[1]) + fabsf(ra[2] - rb[2]);
            if (d > 0.02f) diff++;
        }
    }
    return diff;
}

TEST(software_render_cube_draws) {
    m3_software* backend = render_cube(0.0f, M3_SHADING_FLAT);
    CHECK(backend != NULL);
    int w, h;
    m3_software_viewport_size(backend, &w, &h);
    CHECK(w == REF_W && h == REF_H);

    size_t nontriv = m3_software_nontrivial_pixel_count(backend);
    CHECK(nontriv > (size_t)(REF_W * REF_H / 40)); /* cube covers a few percent */

    /* center of the cube should be lit brightly */
    float center = pixel_brightness(backend, REF_W / 2, REF_H / 2);
    CHECK(center > 0.2f);

    destroy_backend(backend);
}

TEST(software_render_rotation_changes_frame) {
    m3_software* a = render_cube(0.0f, M3_SHADING_FLAT);
    m3_software* b = render_cube(45.0f, M3_SHADING_FLAT);
    CHECK(a != NULL && b != NULL);
    size_t diff = differing_pixels(a, b);
    CHECK(diff > (size_t)(REF_W * REF_H / 100)); /* rotation must visibly change the frame */
    destroy_backend(a);
    destroy_backend(b);
}

TEST(software_render_flat_vs_gouraud) {
    m3_software* flat = render_cube(0.0f, M3_SHADING_FLAT);
    m3_software* gouraud = render_cube(0.0f, M3_SHADING_GOURAUD);
    CHECK(flat != NULL && gouraud != NULL);

    size_t nontriv_flat = m3_software_nontrivial_pixel_count(flat);
    size_t nontriv_gouraud = m3_software_nontrivial_pixel_count(gouraud);
    CHECK(nontriv_flat > 0);
    CHECK(nontriv_gouraud > 0);

    /* the two shading modes must render the cube (visible pixels) and
       both brighten the center */
    float flat_center = pixel_brightness(flat, REF_W / 2, REF_H / 2);
    float gouraud_center = pixel_brightness(gouraud, REF_W / 2, REF_H / 2);
    CHECK(flat_center > 0.0f);
    CHECK(gouraud_center > 0.0f);

    destroy_backend(flat);
    destroy_backend(gouraud);
}

TEST(software_render_lit_vs_sky) {
    /* cube area should be bright; far corners should be dark sky */
    m3_software* backend = render_cube(0.0f, M3_SHADING_FLAT);
    int w, h;
    m3_software_viewport_size(backend, &w, &h);
    float center = pixel_brightness(backend, w / 2, h / 2);
    float corner_l = pixel_brightness(backend, 2, 2);
    float corner_r = pixel_brightness(backend, w - 3, h - 3);
    CHECK(center > 0.3f);
    CHECK(corner_l < 0.1f);
    CHECK(corner_r < 0.1f);
    destroy_backend(backend);
}

TEST(software_render_ppm_export) {
    m3_software* backend = render_cube(30.0f, M3_SHADING_FLAT);
    CHECK(backend != NULL);
    int w, h;
    m3_software_viewport_size(backend, &w, &h);
    CHECK(w == REF_W && h == REF_H);

    const char* path = "/tmp/mini3_test_render.ppm";
    CHECK(m3_software_write_ppm(backend, path) == 1);
    destroy_backend(backend);

    /* verify header of the produced PPM */
    FILE* f = fopen(path, "rb");
    CHECK(f != NULL);
    char header[32] = { 0 };
    if (f) {
        size_t n = fread(header, 1, sizeof(header) - 1, f);
        header[n] = 0;
        fclose(f);
    }
    CHECK(strncmp(header, "P6\n", 3) == 0);
    CHECK(strstr(header + 3, "96 96") != NULL);
}

TEST_MAIN(RUN(software_render_cube_draws), RUN(software_render_rotation_changes_frame),
          RUN(software_render_flat_vs_gouraud), RUN(software_render_lit_vs_sky),
          RUN(software_render_ppm_export));
