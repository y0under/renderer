#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>
#include <vector>

struct GLFWwindow;

namespace gfx {

class Context;

class Swapchain {
public:
  Swapchain() = default;
  ~Swapchain();

  Swapchain(Swapchain const&) = delete;
  Swapchain& operator=(Swapchain const&) = delete;

  Swapchain(Swapchain&& other) noexcept;
  Swapchain& operator=(Swapchain&& other) noexcept;

  void init(Context const& ctx, GLFWwindow* window);
  void shutdown(Context const& ctx);

  // Minimal entry point for later: call when framebuffer size changes.
  void recreate(Context const& ctx, GLFWwindow* window);

  VkSwapchainKHR handle() const { return swapchain_; }
  VkFormat image_format() const { return image_format_; }
  VkExtent2D extent() const { return extent_; }

  std::vector<VkImage> const& images() const { return images_; }
  std::vector<VkImageView> const& image_views() const { return image_views_; }

private:
  void create_swapchain(Context const& ctx, GLFWwindow* window, VkSwapchainKHR old_swapchain);
  void create_image_views(Context const& ctx);
  void destroy_image_views(Context const& ctx);

private:
  VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
  VkFormat image_format_ = VK_FORMAT_UNDEFINED;
  VkExtent2D extent_{};

  std::vector<VkImage> images_;
  std::vector<VkImageView> image_views_;
};

} // namespace gfx
