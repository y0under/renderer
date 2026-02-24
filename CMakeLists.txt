cmake_minimum_required(VERSION 3.20)
project(GameEngine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(glfw3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(Vulkan REQUIRED)

add_executable(app
  src/gfx/Buffer.cc
  src/gfx/Context.cc
  src/gfx/Mesh.cc
  src/gfx/Pipeline.cc
  src/gfx/Renderer.cc
  src/gfx/Swapchain.cc
  src/main.cc
  src/math/Camera.cc)

target_include_directories(app PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(app PRIVATE
  glfw
  Vulkan::Vulkan)

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|AppleClang|GNU")
  target_compile_options(app PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -pedantic-errors)
endif()


# ---- Shaders (GLSL -> SPIR-V) ------------------------------------------------

find_program(GLSLC glslc HINTS ENV VULKAN_SDK PATH_SUFFIXES bin)
if (NOT GLSLC)
  message(FATAL_ERROR "glslc not found. Ensure Vulkan SDK is installed and glslc is in PATH.")
endif()

set(SHADER_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/shaders)
set(SHADER_BIN_DIR ${CMAKE_CURRENT_BINARY_DIR}/shaders/compiled)
file(MAKE_DIRECTORY ${SHADER_BIN_DIR})

set(VERT_SRC ${SHADER_SRC_DIR}/mesh.vert)
set(FRAG_SRC ${SHADER_SRC_DIR}/mesh.frag)
set(VERT_SPV ${SHADER_BIN_DIR}/mesh.vert.spv)
set(FRAG_SPV ${SHADER_BIN_DIR}/mesh.frag.spv)

add_custom_command(
  OUTPUT ${VERT_SPV}
  COMMAND ${GLSLC} -o ${VERT_SPV} ${VERT_SRC}
  DEPENDS ${VERT_SRC}
  VERBATIM
)

add_custom_command(
  OUTPUT ${FRAG_SPV}
  COMMAND ${GLSLC} -o ${FRAG_SPV} ${FRAG_SRC}
  DEPENDS ${FRAG_SRC}
  VERBATIM
)

add_custom_target(shaders ALL DEPENDS ${VERT_SPV} ${FRAG_SPV})
add_dependencies(app shaders)

target_compile_definitions(app PRIVATE
  GFX_SHADER_VERT_PATH="${VERT_SPV}"
  GFX_SHADER_FRAG_PATH="${FRAG_SPV}"
)
