#include "math/Camera.h"

#include <glm/gtc/matrix_transform.hpp>

namespace math {

void Camera::set_perspective(float fovy_radians, float near_z, float far_z) {
  fovy_ = fovy_radians;
  near_z_ = near_z;
  far_z_ = far_z;
}

void Camera::set_look_at(glm::vec3 eye, glm::vec3 center, glm::vec3 up) {
  eye_ = eye;
  center_ = center;
  up_ = up;
}

glm::mat4 Camera::mvp(float aspect, glm::mat4 const& model) const {
  glm::mat4 const view = glm::lookAt(eye_, center_, up_);
  glm::mat4 proj = glm::perspective(fovy_, aspect, near_z_, far_z_);

  // Vulkan NDC: Y inverted compared to OpenGL conventions used by glm::perspective.
  proj[1][1] *= -1.0f;

  return proj * view * model;
}

} // namespace math
