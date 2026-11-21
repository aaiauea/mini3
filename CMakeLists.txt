cmake_minimum_required(VERSION 3.20)
project(mini3 VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Math library: "m" on Unix; no find_package needed for a first cut

add_compile_options(-Wall -Wextra)

# ---------------------------------------------------------------- core
add_library(m3core STATIC
  src/m3_math.c
  src/m3_scene.c
  src/m3_geometry.c
  src/m3_material.c
  src/m3_light.c
  src/m3_camera.c
  src/m3_renderer.c
  src/m3_frame.c
)
target_include_directories(m3core PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(m3core PUBLIC m)
# math is part of the core library (m3_math.c) - no separate target.

# ---------------------------------------------------------------- backends
add_library(m3mock STATIC backends/mock/m3_mock_backend.c)
target_include_directories(m3mock PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(m3mock PUBLIC m3core)

add_library(m3software STATIC backends/software/m3_software.c)
target_include_directories(m3software PUBLIC ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/backends)
target_link_libraries(m3software PUBLIC m3core)

# ---------------------------------------------------------------- example
add_executable(cube_native examples/cube_native/main.c)
target_include_directories(cube_native PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(cube_native PRIVATE m3software m3core)

# ---------------------------------------------------------------- tests
set(M3_TESTS
  test_math
  test_scene
  test_geometry_material
  test_camera
  test_frame
  test_mock_backend
  test_software_render
)

install(TARGETS cube_native m3core m3mock m3software
        ARCHIVE DESTINATION lib
        RUNTIME DESTINATION bin)

foreach(t IN LISTS M3_TESTS)
  add_executable(${t} tests/${t}.c)
  target_include_directories(${t} PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/tests ${CMAKE_SOURCE_DIR}/backends ${CMAKE_SOURCE_DIR})
  target_link_libraries(${t} PRIVATE m3core m3mock m3software)
  add_test(NAME ${t} COMMAND ${t})
endforeach()

enable_testing()
