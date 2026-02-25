#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>

namespace gfx {

class Context;

class Image {
public:
  Image() = default;
  ~Image();

  Image(Image const&) = delete;
  Image& operator=(Image const&) = delete;

  Image(Image&& other) noexcept;
  Image& operator=(Image&& other) noexcept;

  void init_2d(Context const& ctx,
               VkExtent2D extent,
               VkFormat format,
               VkImageUsageFlags usage,
               VkImageAspectFlags aspect);

  void shutdown(Context const& ctx);

  VkImage image() const { return image_; }
  VkImageView view() const { return view_; }
  VkFormat format() const { return format_; }
  VkExtent2D extent() const { return extent_; }

private:
  VkImage image_ = VK_NULL_HANDLE;
  VkDeviceMemory memory_ = VK_NULL_HANDLE;
  VkImageView view_ = VK_NULL_HANDLE;

  VkFormat format_ = VK_FORMAT_UNDEFINED;
  VkExtent2D extent_{};
};

} // namespace gfx
