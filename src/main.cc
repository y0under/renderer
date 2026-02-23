#include <GLFW/glfw3.h>

#include <cstdio>
#include <cstdlib>

#include "gfx/Context.h"
#include "gfx/Pipeline.h"
#include "gfx/Renderer.h"
#include "gfx/Swapchain.h"

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

  try {
    gfx::ContextCreateInfo ci{};
    ci.enable_validation = true;
    ci.enable_debug_utils = true;
    ctx.init(window, ci);

    sc.init(ctx, window);

    pl.init(ctx, sc, shader_vert_path(), shader_frag_path());

    rd.init(ctx, sc, pl);
  } catch (std::exception const& e) {
    std::fprintf(stderr, "Init failed: %s\n", e.what());
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
    (void)rd.draw_frame(ctx, window, sc, pl);
  }

  rd.shutdown(ctx);
  pl.shutdown(ctx);
  sc.shutdown(ctx);
  ctx.shutdown();

  glfwDestroyWindow(window);
  glfwTerminate();
  return 0;
}
