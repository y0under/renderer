#include "gfx/Swapchain.h"

#include "gfx/Context.h"
#include "gfx/GlfwVulkan.h"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace gfx {

namespace {

[[noreturn]] void fail(char const* msg) {
  throw std::runtime_error(msg);
}

[[noreturn]] void fail(std::string const& msg) {
  throw std::runtime_error(msg);
}

void vk_check(VkResult r, char const* what) {
  if (r == VK_SUCCESS) {
    return;
  }
  fail(std::string("Vulkan error: ") + what + " (" + std::to_string(static_cast<int>(r)) + ")");
}

struct SwapchainSupport {
  VkSurfaceCapabilitiesKHR caps{};
  std::vector<VkSurfaceFormatKHR> formats;
  std::vector<VkPresentModeKHR> present_modes;
};

SwapchainSupport query_swapchain_support(VkPhysicalDevice pd, VkSurfaceKHR surface) {
  SwapchainSupport s{};

  vk_check(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(pd, surface, &s.caps), "vkGetPhysicalDeviceSurfaceCapabilitiesKHR");

  uint32_t fmt_count = 0;
  vk_check(vkGetPhysicalDeviceSurfaceFormatsKHR(pd, surface, &fmt_count, nullptr), "vkGetPhysicalDeviceSurfaceFormatsKHR(count)");
  s.formats.resize(fmt_count);
  if (fmt_count != 0U) {
    vk_check(vkGetPhysicalDeviceSurfaceFormatsKHR(pd, surface, &fmt_count, s.formats.data()), "vkGetPhysicalDeviceSurfaceFormatsKHR(list)");
  }

  uint32_t pm_count = 0;
  vk_check(vkGetPhysicalDeviceSurfacePresentModesKHR(pd, surface, &pm_count, nullptr), "vkGetPhysicalDeviceSurfacePresentModesKHR(count)");
  s.present_modes.resize(pm_count);
  if (pm_count != 0U) {
    vk_check(vkGetPhysicalDeviceSurfacePresentModesKHR(pd, surface, &pm_count, s.present_modes.data()), "vkGetPhysicalDeviceSurfacePresentModesKHR(list)");
  }

  return s;
}

VkSurfaceFormatKHR choose_surface_format(std::vector<VkSurfaceFormatKHR> const& formats) {
  if (formats.empty()) {
    fail("No surface formats available.");
  }

  // Prefer SRGB 8-bit BGRA if available (common on macOS/MoltenVK).
  for (auto const& f : formats) {
    if (f.format == VK_FORMAT_B8G8R8A8_SRGB && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
      return f;
    }
  }

  // Fallback: first.
  return formats.front();
}

VkPresentModeKHR choose_present_mode(std::vector<VkPresentModeKHR> const& modes) {
  // Always available: FIFO. Prefer MAILBOX if offered.
  for (auto m : modes) {
    if (m == VK_PRESENT_MODE_MAILBOX_KHR) {
      return m;
    }
  }
  return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D clamp_extent(VkExtent2D value, VkExtent2D minv, VkExtent2D maxv) {
  VkExtent2D out{};
  out.width = std::clamp(value.width, minv.width, maxv.width);
  out.height = std::clamp(value.height, minv.height, maxv.height);
  return out;
}

VkExtent2D choose_extent(GLFWwindow* window, VkSurfaceCapabilitiesKHR const& caps) {
  if (caps.currentExtent.width != UINT32_MAX) {
    return caps.currentExtent;
  }

  int w = 0;
  int h = 0;
  glfwGetFramebufferSize(window, &w, &h);

  if (w <= 0 || h <= 0) {
    // This can happen on minimize; caller should handle by waiting.
    VkExtent2D e{};
    e.width = 1;
    e.height = 1;
    return clamp_extent(e, caps.minImageExtent, caps.maxImageExtent);
  }

  VkExtent2D e{};
  e.width = static_cast<uint32_t>(w);
  e.height = static_cast<uint32_t>(h);
  return clamp_extent(e, caps.minImageExtent, caps.maxImageExtent);
}

} // namespace

Swapchain::~Swapchain() {
  // Note: requires Context to destroy; do not auto-destroy here.
}

Swapchain::Swapchain(Swapchain&& other) noexcept {
  *this = std::move(other);
}

Swapchain& Swapchain::operator=(Swapchain&& other) noexcept {
  if (this == &other) {
    return *this;
  }

  swapchain_ = other.swapchain_;
  image_format_ = other.image_format_;
  extent_ = other.extent_;
  images_ = std::move(other.images_);
  image_views_ = std::move(other.image_views_);

  other.swapchain_ = VK_NULL_HANDLE;
  other.image_format_ = VK_FORMAT_UNDEFINED;
  other.extent_ = {};
  other.images_.clear();
  other.image_views_.clear();

  return *this;
}

void Swapchain::init(Context const& ctx, GLFWwindow* window) {
  if (window == nullptr) {
    fail("Swapchain::init: window == nullptr");
  }
  create_swapchain(ctx, window, VK_NULL_HANDLE);
  create_image_views(ctx);
}

void Swapchain::shutdown(Context const& ctx) {
  destroy_image_views(ctx);

  if (swapchain_ != VK_NULL_HANDLE) {
    vkDestroySwapchainKHR(ctx.device(), swapchain_, nullptr);
    swapchain_ = VK_NULL_HANDLE;
  }

  images_.clear();
  image_format_ = VK_FORMAT_UNDEFINED;
  extent_ = {};
}

void Swapchain::recreate(Context const& ctx, GLFWwindow* window) {
  if (window == nullptr) {
    fail("Swapchain::recreate: window == nullptr");
  }

  // Wait for valid framebuffer size (e.g., after un-minimize).
  int w = 0;
  int h = 0;
  do {
    glfwGetFramebufferSize(window, &w, &h);
    if (w <= 0 || h <= 0) {
      glfwWaitEvents();
    }
  } while (w <= 0 || h <= 0);

  vk_check(vkDeviceWaitIdle(ctx.device()), "vkDeviceWaitIdle");

  destroy_image_views(ctx);

  VkSwapchainKHR old = swapchain_;
  create_swapchain(ctx, window, old);
  create_image_views(ctx);

  if (old != VK_NULL_HANDLE) {
    vkDestroySwapchainKHR(ctx.device(), old, nullptr);
  }
}

void Swapchain::create_swapchain(Context const& ctx, GLFWwindow* window, VkSwapchainKHR old_swapchain) {
  auto const support = query_swapchain_support(ctx.physical_device(), ctx.surface());
  auto const surface_format = choose_surface_format(support.formats);
  auto const present_mode = choose_present_mode(support.present_modes);
  auto const chosen_extent = choose_extent(window, support.caps);

  uint32_t image_count = support.caps.minImageCount + 1U;
  if (support.caps.maxImageCount != 0U && image_count > support.caps.maxImageCount) {
    image_count = support.caps.maxImageCount;
  }

  VkSwapchainCreateInfoKHR ci{};
  ci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  ci.surface = ctx.surface();
  ci.minImageCount = image_count;
  ci.imageFormat = surface_format.format;
  ci.imageColorSpace = surface_format.colorSpace;
  ci.imageExtent = chosen_extent;
  ci.imageArrayLayers = 1;
  ci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

  uint32_t const qf_indices[] = {ctx.graphics_queue_family(), ctx.present_queue_family()};
  if (ctx.graphics_queue_family() != ctx.present_queue_family()) {
    ci.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
    ci.queueFamilyIndexCount = 2;
    ci.pQueueFamilyIndices = qf_indices;
  } else {
    ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ci.queueFamilyIndexCount = 0;
    ci.pQueueFamilyIndices = nullptr;
  }

  ci.preTransform = support.caps.currentTransform;
  ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  ci.presentMode = present_mode;
  ci.clipped = VK_TRUE;
  ci.oldSwapchain = old_swapchain;

  VkSwapchainKHR new_swapchain = VK_NULL_HANDLE;
  vk_check(vkCreateSwapchainKHR(ctx.device(), &ci, nullptr, &new_swapchain), "vkCreateSwapchainKHR");

  swapchain_ = new_swapchain;
  image_format_ = surface_format.format;
  extent_ = chosen_extent;

  uint32_t actual_count = 0;
  vk_check(vkGetSwapchainImagesKHR(ctx.device(), swapchain_, &actual_count, nullptr), "vkGetSwapchainImagesKHR(count)");

  images_.resize(actual_count);
  vk_check(vkGetSwapchainImagesKHR(ctx.device(), swapchain_, &actual_count, images_.data()), "vkGetSwapchainImagesKHR(list)");

  std::fprintf(stderr, "Swapchain: images=%u, extent=%ux%u\n",
               actual_count, extent_.width, extent_.height);
}

void Swapchain::create_image_views(Context const& ctx) {
  image_views_.resize(images_.size(), VK_NULL_HANDLE);

  for (size_t i = 0; i < images_.size(); ++i) {
    VkImageViewCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    ci.image = images_[i];
    ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    ci.format = image_format_;
    ci.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    ci.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    ci.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    ci.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

    ci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    ci.subresourceRange.baseMipLevel = 0;
    ci.subresourceRange.levelCount = 1;
    ci.subresourceRange.baseArrayLayer = 0;
    ci.subresourceRange.layerCount = 1;

    vk_check(vkCreateImageView(ctx.device(), &ci, nullptr, &image_views_[i]), "vkCreateImageView");
  }
}

void Swapchain::destroy_image_views(Context const& ctx) {
  for (auto& v : image_views_) {
    if (v != VK_NULL_HANDLE) {
      vkDestroyImageView(ctx.device(), v, nullptr);
      v = VK_NULL_HANDLE;
    }
  }
  image_views_.clear();
}

} // namespace gfx
