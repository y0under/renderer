#pragma once

#include <vulkan/vulkan.h>

namespace gfx {

class Context;
class Image;
class Swapchain;

class Depth {
public:
  Depth() = default;
  ~Depth();

  Depth(Depth const&) = delete;
  Depth& operator=(Depth const&) = delete;

  Depth(Depth&& other) noexcept;
  Depth& operator=(Depth&& other) noexcept;

  void init(Context const& ctx, Swapchain const& sc);
  void shutdown(Context const& ctx);

  VkFormat format() const { return format_; }
  VkImageView view() const;

private:
  VkFormat pick_format(Context const& ctx) const;

private:
  Image* image_ = nullptr;
  VkFormat format_ = VK_FORMAT_UNDEFINED;
};

} // namespace gfx
