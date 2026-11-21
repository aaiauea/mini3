# MINI3 - Renderer-Agnostic 3D Engine
#
# Usage:
#   make test        - build & run all native test suites
#   make demo        - build & run the native rotating-cube demo (software backend)
#   make all         - test + demo
#   make clean       - remove built artifacts
#
# The WebGL 2 demo (demo/webgl2/) is static HTML/JS and needs a browser;
# see docs/WEBGL2_DEMO.md.

CC        ?= cc
CFLAGS    ?= -std=c11 -Wall -Wextra -O2
CPPFLAGS  ?= -I include -I tests -I backends
LDLIBS    ?= -lm

CORE_SRC   := src/m3_math.c src/m3_scene.c src/m3_geometry.c src/m3_material.c \
              src/m3_light.c src/m3_camera.c src/m3_renderer.c src/m3_frame.c
MOCK_SRC   := backends/mock/m3_mock_backend.c
SW_SRC     := backends/software/m3_software.c

TESTS := math scene geometry_material camera frame mock_backend software_render

.PHONY: all test demo clean

all: test demo

test:
	@for t in $(TESTS); do \
	  echo "== $$t =="; \
	  $(CC) $(CFLAGS) $(CPPFLAGS) tests/test_$$t.c $(CORE_SRC) backends/mock/m3_mock_backend.c backends/software/m3_software.c $(LDLIBS) -o /tmp/test_$$t && /tmp/test_$$t; \
	done

demo: examples/cube_native/main.c $(CORE_SRC) backends/software/m3_software.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -I . examples/cube_native/main.c backends/software/m3_software.c $(CORE_SRC) $(LDLIBS) -o /tmp/cube_native
	/tmp/cube_native 3 30 128 128 out

clean:
	rm -rf /tmp/test_math /tmp/test_scene /tmp/test_geometry_material /tmp/test_camera \
	       /tmp/test_frame /tmp/test_mock_backend /tmp/test_software_render \
	       /tmp/cube_native out
