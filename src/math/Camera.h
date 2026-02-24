#pragma once

#include <glm/glm.hpp>

namespace math {

class Camera {
public:
  Camera() = default;

  void set_perspective(float fovy_radians, float near_z, float far_z);
  void set_look_at(glm::vec3 eye, glm::vec3 center, glm::vec3 up);

  // Vulkan clip space adjustment included (proj[1][1] *= -1).
  glm::mat4 mvp(float aspect, glm::mat4 const& model) const;

private:
  float fovy_ = 1.0f;
  float near_z_ = 0.1f;
  float far_z_ = 100.0f;

  glm::vec3 eye_{0.0f, 0.0f, 2.0f};
  glm::vec3 center_{0.0f, 0.0f, 0.0f};
  glm::vec3 up_{0.0f, 1.0f, 0.0f};
};

} // namespace math
