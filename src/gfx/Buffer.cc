#include "gfx/Buffer.h"

#include "gfx/Context.h"

#include <cstring>
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

  fail("No suitable memory type found.");
}

} // namespace

Buffer::~Buffer() {
}

Buffer::Buffer(Buffer&& other) noexcept {
  *this = std::move(other);
}

Buffer& Buffer::operator=(Buffer&& other) noexcept {
  if (this == &other) {
    return *this;
  }

  buffer_ = other.buffer_;
  memory_ = other.memory_;
  size_ = other.size_;

  other.buffer_ = VK_NULL_HANDLE;
  other.memory_ = VK_NULL_HANDLE;
  other.size_ = 0;

  return *this;
}

void Buffer::init(Context const& ctx,
                  VkDeviceSize size,
                  VkBufferUsageFlags usage,
                  VkMemoryPropertyFlags memory_props) {
  if (size == 0) {
    fail("Buffer::init: size == 0");
  }

  VkBufferCreateInfo bci{};
  bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bci.size = size;
  bci.usage = usage;
  bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  vk_check(vkCreateBuffer(ctx.device(), &bci, nullptr, &buffer_), "vkCreateBuffer");

  VkMemoryRequirements req{};
  vkGetBufferMemoryRequirements(ctx.device(), buffer_, &req);

  VkMemoryAllocateInfo mai{};
  mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  mai.allocationSize = req.size;
  mai.memoryTypeIndex = find_memory_type(ctx, req.memoryTypeBits, memory_props);

  vk_check(vkAllocateMemory(ctx.device(), &mai, nullptr, &memory_), "vkAllocateMemory");
  vk_check(vkBindBufferMemory(ctx.device(), buffer_, memory_, 0), "vkBindBufferMemory");

  size_ = size;
}

void Buffer::shutdown(Context const& ctx) {
  if (buffer_ != VK_NULL_HANDLE) {
    vkDestroyBuffer(ctx.device(), buffer_, nullptr);
    buffer_ = VK_NULL_HANDLE;
  }
  if (memory_ != VK_NULL_HANDLE) {
    vkFreeMemory(ctx.device(), memory_, nullptr);
    memory_ = VK_NULL_HANDLE;
  }
  size_ = 0;
}

void Buffer::upload(Context const& ctx, void const* data, std::size_t size, std::size_t offset) {
  if (data == nullptr) {
    fail("Buffer::upload: data == nullptr");
  }
  if (offset + size > static_cast<std::size_t>(size_)) {
    fail("Buffer::upload: out of range");
  }

  void* mapped = nullptr;
  vk_check(vkMapMemory(ctx.device(), memory_, static_cast<VkDeviceSize>(offset), static_cast<VkDeviceSize>(size), 0, &mapped),
           "vkMapMemory");
  std::memcpy(mapped, data, size);
  vkUnmapMemory(ctx.device(), memory_);
}

} // namespace gfx
