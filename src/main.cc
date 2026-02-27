#include <GLFW/glfw3.h>

#include <cstdio>
#include <cstdlib>

#include "assets/ObjLoader.h"
#include "gfx/Context.h"
#include "gfx/Depth.h"
#include "gfx/Mesh.h"
#include "gfx/Pipeline.h"
#include "gfx/Renderer.h"
#include "gfx/Swapchain.h"
#include "math/Camera.h"

#include <glm/glm.hpp>

namespace {

[[noreturn]] void die(char const* msg) {
  std::fprintf(stderr, "%s\n", msg);
  std::fflush(stderr);
  std::exit(EXIT_FAILURE);
}

void set_glfw_callbacks() {
  glfwSetErrorCallback([](int code, char const* desc) {
    std::fprintf(stderr, "GLFW error (%d): %s\n", code, (desc != nullptr) ? desc : "(null)");
  });
}

char const* shader_vert_path() {
#ifdef GFX_SHADER_VERT_PATH
  return GFX_SHADER_VERT_PATH;
#else
  return "shaders/compiled/mesh.vert.spv";
#endif
}

char const* shader_frag_path() {
#ifdef GFX_SHADER_FRAG_PATH
  return GFX_SHADER_FRAG_PATH;
#else
  return "shaders/compiled/mesh.frag.spv";
#endif
}

gfx::Vertex make_vertex(float x, float y, float z) {
  // Minimal: position-based pseudo color (not normalized, but stable).
  float const r = 0.5f + 0.5f * x;
  float const g = 0.5f + 0.5f * y;
  float const b = 0.5f + 0.5f * z;
  return gfx::Vertex{{x, y, z}, {r, g, b}};
}

} // namespace

int main() {
  set_glfw_callbacks();

  if (glfwInit() == GLFW_FALSE) {
    die("glfwInit() failed.");
  }

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

  GLFWwindow* window = glfwCreateWindow(1280, 720, "Renderer", nullptr, nullptr);
  if (window == nullptr) {
    glfwTerminate();
    die("glfwCreateWindow() failed.");
  }

  gfx::Context ctx{};
  gfx::Swapchain sc{};
  gfx::Depth depth{};
  gfx::Pipeline pl{};
  gfx::Renderer rd{};
  gfx::Mesh mesh{};
  math::Camera cam{};

  try {
    gfx::ContextCreateInfo ci{};
    ci.enable_validation = true;
    ci.enable_debug_utils = true;
    ctx.init(window, ci);

    sc.init(ctx, window);

    depth.init(ctx, sc);

    pl.init(ctx, sc, depth.format(), shader_vert_path(), shader_frag_path());

    rd.init(ctx, sc, pl, depth);

    // ---- Load OBJ (v/f only) ----
    // Place your obj at: assets/model.obj
    assets::ObjMesh const om = assets::ObjLoader::load("assets/model.obj");

    std::size_t const vcount = om.positions_xyz.size() / 3U;
    std::vector<gfx::Vertex> vertices;
    vertices.reserve(vcount);

    for (std::size_t i = 0; i < vcount; ++i) {
      float const x = om.positions_xyz[i * 3 + 0];
      float const y = om.positions_xyz[i * 3 + 1];
      float const z = om.positions_xyz[i * 3 + 2];
      vertices.push_back(make_vertex(x, y, z));
    }

    mesh.init_from_data(ctx, ctx.uploader(), vertices, om.indices);

    cam.set_perspective(60.0f * 3.1415926535f / 180.0f, 0.1f, 100.0f);
    cam.set_look_at(glm::vec3(0.0f, 0.0f, 3.0f),
                    glm::vec3(0.0f, 0.0f, 0.0f),
                    glm::vec3(0.0f, 1.0f, 0.0f));
  } catch (std::exception const& e) {
    std::fprintf(stderr, "Init failed: %s\n", e.what());
    mesh.shutdown(ctx);
    rd.shutdown(ctx);
    pl.shutdown(ctx);
    depth.shutdown(ctx);
    sc.shutdown(ctx);
    ctx.shutdown();
    glfwDestroyWindow(window);
    glfwTerminate();
    return EXIT_FAILURE;
  }

  while (glfwWindowShouldClose(window) == GLFW_FALSE) {
    glfwPollEvents();
    (void)rd.draw_frame(ctx, window, sc, pl, mesh, cam, depth);
  }

  mesh.shutdown(ctx);
  rd.shutdown(ctx);
  pl.shutdown(ctx);
  depth.shutdown(ctx);
  sc.shutdown(ctx);
  ctx.shutdown();

  glfwDestroyWindow(window);
  glfwTerminate();
  return 0;
}
