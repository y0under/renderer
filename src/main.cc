#include <GLFW/glfw3.h>

#include <cstdio>
#include <cstdlib>
#include <vector>

#include "assets/ObjLoader.h"
#include "gfx/Context.h"
#include "gfx/Depth.h"
#include "gfx/Mesh.h"
#include "gfx/Pipeline.h"
#include "gfx/Renderer.h"
#include "gfx/Swapchain.h"
#include "math/Camera.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

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

void update_angles(GLFWwindow* window, float dt, float& yaw, float& pitch) {
  float const speed = 1.5f; // rad/s

  if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
    yaw += speed * dt;
  }
  if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
    yaw -= speed * dt;
  }
  if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
    pitch += speed * dt;
  }
  if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
    pitch -= speed * dt;
  }

  float const limit = 1.4f;
  if (pitch > limit) {
    pitch = limit;
  }
  if (pitch < -limit) {
    pitch = -limit;
  }
}

float update_scale(GLFWwindow* window, float dt, float scale) {
  float const speed = 1.5f; // units per second (multiplicative-ish)
  float const factor = 1.0f + speed * dt;

  if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) {
    scale *= factor;
  }
  if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) {
    scale /= factor;
  }

  float const min_s = 0.01f;
  float const max_s = 1000.0f;

  if (scale < min_s) {
    scale = min_s;
  }
  if (scale > max_s) {
    scale = max_s;
  }

  return scale;
}

glm::mat4 make_model_matrix(float yaw, float pitch, float scale) {
  glm::mat4 model(1.0f);

  model = glm::scale(model, glm::vec3(scale, scale, scale));
  model = glm::rotate(model, yaw, glm::vec3(0.0f, 1.0f, 0.0f));
  model = glm::rotate(model, pitch, glm::vec3(1.0f, 0.0f, 0.0f));

  return model;
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

    assets::ObjIndexedMesh const om = assets::ObjLoader::load("assets/model.obj");

    std::vector<gfx::Vertex> verts;
    verts.reserve(om.vertices.size());
    for (auto const& v : om.vertices) {
      gfx::Vertex gv{};
      gv.pos[0] = v.pos[0];
      gv.pos[1] = v.pos[1];
      gv.pos[2] = v.pos[2];
      gv.normal[0] = v.normal[0];
      gv.normal[1] = v.normal[1];
      gv.normal[2] = v.normal[2];
      gv.uv[0] = v.uv[0];
      gv.uv[1] = v.uv[1];
      verts.push_back(gv);
    }

    mesh.init_from_data(ctx, ctx.uploader(), verts, om.indices);

    cam.set_perspective(60.0f * 3.1415926535f / 180.0f, 0.1f, 100.0f);
    cam.set_look_at(glm::vec3(1.8f, 1.2f, 2.8f),
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

  float yaw = 0.0f;
  float pitch = 0.0f;
  float prev_time = static_cast<float>(glfwGetTime());

  float scale = 1.0f;
  while (glfwWindowShouldClose(window) == GLFW_FALSE) {
    glfwPollEvents();

    float const now = static_cast<float>(glfwGetTime());
    float const dt = now - prev_time;
    prev_time = now;

    update_angles(window, dt, yaw, pitch);
    scale = update_scale(window, dt, scale);

    glm::mat4 const model = make_model_matrix(yaw, pitch, scale);
    (void)rd.draw_frame(ctx, window, sc, pl, mesh, cam, model, depth);
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
