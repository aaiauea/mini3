/*
 * m3_software.h - Mini3 software reference backend
 *
 * A zero-dependency, CPU-based reference backend. It implements the full
 * renderer contract (see m3_renderer.h) but performs all work in software,
 * which makes it ideal for:
 *   - headless tests and CI (no GPU / no GL required)
 *   - verifying the core end-to-end (scene -> frame -> render)
 *   - serving as a small, readable reference for writing backends
 *
 * The backend does NOT window, composite, or present — it writes pixels into
 * an in-memory framebuffer which the caller can inspect or export (PPM).
 */
#ifndef M3_SOFTWARE_H
#define M3_SOFTWARE_H

#include "mini3/m3_renderer.h"

typedef struct m3_software m3_software;

/* The contract implementation. Global, static, no heap. */
extern const m3_renderer_interface m3_software_renderer;

/* Lifecycle */
m3_software* m3_software_create(void);
void        m3_software_destroy(m3_software* backend);
const m3_renderer_interface* m3_software_renderer_get_interface(void);

/* Inspect the framebuffer after end_frame. */
size_t      m3_software_viewport_size(const m3_software* backend, int* w, int* h);
/* Returns 1 and fills rgb[0..2] (0..1 floats) if the pixel exists, else 0. */
int         m3_software_get_pixel(const m3_software* backend, int x, int y, float rgb[3]);
/* Counts pixels that differ from the clear color by more than ~0.02 (i.e. the
 * scene is actually drawn). */
size_t      m3_software_nontrivial_pixel_count(const m3_software* backend);

/* Export the current framebuffer as a P6 (binary) PPM file. 1 on success. */
int         m3_software_write_ppm(const m3_software* backend, const char* path);

#endif /* M3_SOFTWARE_H */
