#include "gfx/Depth.h"

#include "gfx/Context.h"
#include "gfx/Image.h"
#include "gfx/Swapchain.h"

#include <new>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace gfx {

namespace {

[[noreturn]] void fail(char const* msg) { throw std::runtime_error(msg); }
[[noreturn]] void fail(std::string const& msg) { throw std::runtime_error(msg); }

void vk_check(VkResult r, char const* what) {
  if (r != VK_SUCCESS) {
    fail(std::string("Vulkan error: ") + what + " (" + std::to_string(static_cast<int>(r)) + ")");
  }
}

bool has_stencil(VkFormat fmt) {
  return (fmt == VK_FORMAT_D32_SFLOAT_S8_UINT)
      || (fmt == VK_FORMAT_D24_UNORM_S8_UINT);
}

} // namespace

Depth::~Depth() {
}

Depth::Depth(Depth&& other) noexcept {
  *this = std::move(other);
}

Depth& Depth::operator=(Depth&& other) noexcept {
  if (this == &other) {
    return *this;
  }

  image_ = other.image_;
  format_ = other.format_;

  other.image_ = nullptr;
  other.format_ = VK_FORMAT_UNDEFINED;

  return *this;
}

VkFormat Depth::pick_format(Context const& ctx) const {
  VkFormat candidates[] = {
    VK_FORMAT_D32_SFLOAT,
    VK_FORMAT_D32_SFLOAT_S8_UINT,
    VK_FORMAT_D24_UNORM_S8_UINT,
  };

  for (auto fmt : candidates) {
    VkFormatProperties props{};
    vkGetPhysicalDeviceFormatProperties(ctx.physical_device(), fmt, &props);

    bool const ok = (props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0u;
    if (ok) {
      return fmt;
    }
  }

  fail("No suitable depth format found.");
}

void Depth::init(Context const& ctx, Swapchain const& sc) {
  if (image_ != nullptr) {
    fail("Depth::init called twice");
  }

  format_ = pick_format(ctx);

  image_ = new (std::nothrow) Image();
  if (image_ == nullptr) {
    fail("Depth: allocation failed");
  }

  VkImageAspectFlags aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
  if (has_stencil(format_)) {
    aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
  }

  image_->init_2d(ctx,
                  sc.extent(),
                  format_,
                  VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                  aspect);
}

void Depth::shutdown(Context const& ctx) {
  if (image_ != nullptr) {
    image_->shutdown(ctx);
    delete image_;
    image_ = nullptr;
  }
  format_ = VK_FORMAT_UNDEFINED;
}

VkImageView Depth::view() const {
  return (image_ != nullptr) ? image_->view() : VK_NULL_HANDLE;
}

} // namespace gfx
