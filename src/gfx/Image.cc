#include "gfx/Image.h"

#include "gfx/Context.h"

#include <stdexcept>
#include <string>
#include <utility>

namespace gfx {

namespace {

[[noreturn]] void fail(char const* msg) { throw std::runtime_error(msg); }
[[noreturn]] void fail(std::string const& msg) { throw std::runtime_error(msg); }

void vk_check(VkResult r, char const* what) {
  if (r != VK_SUCCESS) {
    fail(std::string("Vulkan error: ") + what + " (" + std::to_string(static_cast<int>(r)) + ")");
  }
}

uint32_t find_memory_type(Context const& ctx, uint32_t type_bits, VkMemoryPropertyFlags props) {
  VkPhysicalDeviceMemoryProperties mem{};
  vkGetPhysicalDeviceMemoryProperties(ctx.physical_device(), &mem);

  for (uint32_t i = 0; i < mem.memoryTypeCount; ++i) {
    bool const type_ok = ((type_bits & (1u << i)) != 0u);
    bool const props_ok = ((mem.memoryTypes[i].propertyFlags & props) == props);
    if (type_ok && props_ok) {
      return i;
    }
  }

  fail("No suitable memory type found for image.");
}

} // namespace

Image::~Image() {
}

Image::Image(Image&& other) noexcept {
  *this = std::move(other);
}

Image& Image::operator=(Image&& other) noexcept {
  if (this == &other) {
    return *this;
  }

  image_ = other.image_;
  memory_ = other.memory_;
  view_ = other.view_;
  format_ = other.format_;
  extent_ = other.extent_;

  other.image_ = VK_NULL_HANDLE;
  other.memory_ = VK_NULL_HANDLE;
  other.view_ = VK_NULL_HANDLE;
  other.format_ = VK_FORMAT_UNDEFINED;
  other.extent_ = {};

  return *this;
}

void Image::init_2d(Context const& ctx,
                    VkExtent2D extent,
                    VkFormat format,
                    VkImageUsageFlags usage,
                    VkImageAspectFlags aspect) {
  if (extent.width == 0U || extent.height == 0U) {
    fail("Image::init_2d: invalid extent");
  }

  extent_ = extent;
  format_ = format;

  VkImageCreateInfo ici{};
  ici.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  ici.imageType = VK_IMAGE_TYPE_2D;
  ici.format = format;
  ici.extent = VkExtent3D{extent.width, extent.height, 1};
  ici.mipLevels = 1;
  ici.arrayLayers = 1;
  ici.samples = VK_SAMPLE_COUNT_1_BIT;
  ici.tiling = VK_IMAGE_TILING_OPTIMAL;
  ici.usage = usage;
  ici.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

  vk_check(vkCreateImage(ctx.device(), &ici, nullptr, &image_), "vkCreateImage");

  VkMemoryRequirements req{};
  vkGetImageMemoryRequirements(ctx.device(), image_, &req);

  VkMemoryAllocateInfo mai{};
  mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  mai.allocationSize = req.size;
  mai.memoryTypeIndex = find_memory_type(ctx, req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  vk_check(vkAllocateMemory(ctx.device(), &mai, nullptr, &memory_), "vkAllocateMemory(image)");
  vk_check(vkBindImageMemory(ctx.device(), image_, memory_, 0), "vkBindImageMemory");

  VkImageViewCreateInfo vci{};
  vci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  vci.image = image_;
  vci.viewType = VK_IMAGE_VIEW_TYPE_2D;
  vci.format = format;
  vci.subresourceRange.aspectMask = aspect;
  vci.subresourceRange.baseMipLevel = 0;
  vci.subresourceRange.levelCount = 1;
  vci.subresourceRange.baseArrayLayer = 0;
  vci.subresourceRange.layerCount = 1;

  vk_check(vkCreateImageView(ctx.device(), &vci, nullptr, &view_), "vkCreateImageView(image)");
}

void Image::shutdown(Context const& ctx) {
  if (view_ != VK_NULL_HANDLE) {
    vkDestroyImageView(ctx.device(), view_, nullptr);
    view_ = VK_NULL_HANDLE;
  }
  if (image_ != VK_NULL_HANDLE) {
    vkDestroyImage(ctx.device(), image_, nullptr);
    image_ = VK_NULL_HANDLE;
  }
  if (memory_ != VK_NULL_HANDLE) {
    vkFreeMemory(ctx.device(), memory_, nullptr);
    memory_ = VK_NULL_HANDLE;
  }
  format_ = VK_FORMAT_UNDEFINED;
  extent_ = {};
}

} // namespace gfx
