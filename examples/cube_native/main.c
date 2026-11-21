#define _POSIX_C_SOURCE 200809L
#include <sys/stat.h>
/*
 * cube_native.c - Mini3 native demo: rotating, lit cube (software backend)
 *
 * Renders a rotating, lit cube through the full engine pipeline:
 *   scene -> frame -> software reference backend -> PPM image.
 *
 * Usage:
 *     cube_native [frames] [angle_deg] [width] [height] [outdir]
 *
 * Examples:
 *     cube_native 3 3 256 256 out
 *     cube_native 5 22.5 320 320 out
 *
 * The demo writes one PPM per frame plus a small summary line to stdout.
 * The output is designed to be compared across runs and across shading modes
 * (see tests/test_software_render.c for the expected invariants).
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "mini3/m3_renderer.h"
#include "mini3/m3_scene.h"
#include "mini3/m3_camera.h"
#include "mini3/m3_geometry.h"
#include "mini3/m3_material.h"
#include "mini3/m3_light.h"
#include "backends/software/m3_software.h"

static void print_usage(const char* argv0) {
    fprintf(stderr, "Usage: %s [frames] [angle_deg] [width] [height] [outdir]\n", argv0);
    fprintf(stderr, "  frames     number of frames to render (default 3)\n");
    fprintf(stderr, "  angle_deg  degrees of rotation per frame (default 30)\n");
    fprintf(stderr, "  width      framebuffer width (default 256)\n");
    fprintf(stderr, "  height     framebuffer height (default 256)\n");
    fprintf(stderr, "  outdir     output directory (default out)\n");
}

int main(int argc, char** argv) {
    int frames = 3;
    float angle_deg = 30.0f;
    int width = 256;
    int height = 256;
    const char* outdir = "out";

    int opt_i = 1;
    if (argc > opt_i) frames = atoi(argv[opt_i++]);
    if (argc > opt_i) angle_deg = (float)atof(argv[opt_i++]);
    if (argc > opt_i) width = atoi(argv[opt_i++]);
    if (argc > opt_i) height = atoi(argv[opt_i++]);
    if (argc > opt_i) outdir = argv[opt_i++];
    if (frames <= 0 || width <= 0 || height <= 0 || width > 8192 || height > 8192) {
        print_usage(argv[0]);
        return 1;
    }

    const m3_renderer_interface* renderer = m3_software_renderer_get_interface();

    /* build the scene: one rotating cube + lights */
    m3_scene* scene = m3_scene_create();
    m3_mesh* mesh = m3_mesh_cube(1.4f);
    m3_material* material = m3_material_create(M3_VEC3(0.85f, 0.55f, 0.45f), M3_SHADING_FLAT);
    m3_node* cube = m3_scene_add_node(scene, "cube");
    m3_node_set_drawable(cube, mesh, material);

    m3_camera* camera = m3_camera_create(50.0f, (float)width / (float)height, 0.1f, 100.0f);
    m3_camera_look_at(camera, M3_VEC3(1.0f, 1.6f, 3.6f), M3_VEC3(0.0f, 0.0f, 0.0f), M3_VEC3(0.0f, 1.0f, 0.0f));

    m3_lighting lights;
    m3_lighting_init(&lights);
    m3_lighting_set_ambient(&lights, M3_VEC3(0.22f, 0.22f, 0.3f));
    m3_lighting_add_directional(&lights, M3_VEC3(0.4f, 1.0f, 0.6f), M3_VEC3(1.0f, 0.95f, 0.9f));

    /* backend resources are created once and reused across frames */
    m3_software* backend = (m3_software*)renderer->create(NULL, NULL);
    if (!backend) {
        fprintf(stderr, "failed to create software backend\n");
        return 1;
    }
    renderer->set_viewport(backend, 0, 0, width, height);

    void* geometry = renderer->create_geometry(backend, mesh);
    void* shader = renderer->create_shader(backend, M3_SHADING_FLAT);
    if (!geometry || !shader) {
        fprintf(stderr, "failed to create backend resources\n");
        m3_software_destroy(backend);
        return 1;
    }

    char path[1024];
    if (mkdir(outdir, 0755) != 0) {
        /* directory may already exist - ignore */
    }

    for (int t = 0; t < frames; t++) {
        /* rotate the cube */
        float angle = angle_deg * (float)t;
        m3_node_set_rotation_euler(cube, 0.0f, angle * M3_PI / 180.0f, 0.0f);

        m3_frame frame;
        if (m3_frame_build(&frame, scene, camera, &lights) != 1) {
            fprintf(stderr, "frame build failed\n");
            continue;
        }
        renderer->begin_frame(backend, &frame);
        for (size_t i = 0; i < frame.draw_count; i++) {
            renderer->submit(backend, &frame, &frame.draws[i], geometry, shader);
        }
        renderer->end_frame(backend, &frame);

        size_t nontriv = m3_software_nontrivial_pixel_count(backend);
        snprintf(path, sizeof(path), "%s/cube_frame_%03d.ppm", outdir, t);
        if (m3_software_write_ppm(backend, path) != 1) {
            fprintf(stderr, "failed to write %s\n", path);
        }
        printf("frame %d  angle=%6.2f deg  visible=%zu px  -> %s\n", t, angle, nontriv, path);

        m3_frame_free(&frame);
    }

    renderer->destroy_geometry(backend, geometry);
    renderer->destroy_shader(backend, shader);
    m3_software_destroy(backend);
    m3_material_destroy(material);
    m3_mesh_destroy(mesh);
    m3_camera_destroy(camera);
    m3_scene_destroy(scene);
    return 0;
}
