#include <GLFW/glfw3.h>

#include <cstdio>
#include <cstdlib>

#include "gfx/Context.h"
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

    pl.init(ctx, sc, shader_vert_path(), shader_frag_path());

    rd.init(ctx, sc, pl);

    mesh.init_triangle(ctx);

    cam.set_perspective(60.0f * 3.1415926535f / 180.0f, 0.1f, 100.0f);
    cam.set_look_at(glm::vec3(0.0f, 0.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
  } catch (std::exception const& e) {
    std::fprintf(stderr, "Init failed: %s\n", e.what());
    mesh.shutdown(ctx);
    rd.shutdown(ctx);
    pl.shutdown(ctx);
    sc.shutdown(ctx);
    ctx.shutdown();
    glfwDestroyWindow(window);
    glfwTerminate();
    return EXIT_FAILURE;
  }

  while (glfwWindowShouldClose(window) == GLFW_FALSE) {
    glfwPollEvents();
    (void)rd.draw_frame(ctx, window, sc, pl, mesh, cam);
  }

  mesh.shutdown(ctx);
  rd.shutdown(ctx);
  pl.shutdown(ctx);
  sc.shutdown(ctx);
  ctx.shutdown();

  glfwDestroyWindow(window);
  glfwTerminate();
  return 0;
}
