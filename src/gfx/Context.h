#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>

struct GLFWwindow;

namespace gfx {

struct ContextCreateInfo {
  bool enable_validation = true;
  bool enable_debug_utils = true;
};

class Context {
public:
  Context() = default;
  ~Context();

  Context(Context const&) = delete;
  Context& operator=(Context const&) = delete;

  Context(Context&& other) noexcept;
  Context& operator=(Context&& other) noexcept;

  void init(GLFWwindow* window, ContextCreateInfo const& info);
  void shutdown();

  VkInstance instance() const { return instance_; }
  VkSurfaceKHR surface() const { return surface_; }
  VkPhysicalDevice physical_device() const { return physical_device_; }
  VkDevice device() const { return device_; }

  VkQueue graphics_queue() const { return graphics_queue_; }
  VkQueue present_queue() const { return present_queue_; }

  uint32_t graphics_queue_family() const { return graphics_queue_family_; }
  uint32_t present_queue_family() const { return present_queue_family_; }

private:
  void create_instance(GLFWwindow* window, ContextCreateInfo const& info);
  void setup_debug(ContextCreateInfo const& info);
  void create_surface(GLFWwindow* window);
  void pick_physical_device();
  void create_device(ContextCreateInfo const& info);

private:
  VkInstance instance_ = VK_NULL_HANDLE;
  VkDebugUtilsMessengerEXT debug_messenger_ = VK_NULL_HANDLE;
  VkSurfaceKHR surface_ = VK_NULL_HANDLE;

  VkPhysicalDevice physical_device_ = VK_NULL_HANDLE;
  VkDevice device_ = VK_NULL_HANDLE;

  VkQueue graphics_queue_ = VK_NULL_HANDLE;
  VkQueue present_queue_ = VK_NULL_HANDLE;

  uint32_t graphics_queue_family_ = UINT32_MAX;
  uint32_t present_queue_family_ = UINT32_MAX;
};

} // namespace gfx
