#pragma once

#include <vulkan/vulkan.h>

#include <cstddef>
#include <cstdint>

namespace gfx {

class Context;

class Buffer {
public:
  Buffer() = default;
  ~Buffer();

  Buffer(Buffer const&) = delete;
  Buffer& operator=(Buffer const&) = delete;

  Buffer(Buffer&& other) noexcept;
  Buffer& operator=(Buffer&& other) noexcept;

  void init(Context const& ctx,
            VkDeviceSize size,
            VkBufferUsageFlags usage,
            VkMemoryPropertyFlags memory_props);

  void shutdown(Context const& ctx);

  void upload(Context const& ctx, void const* data, std::size_t size, std::size_t offset = 0);

  VkBuffer handle() const { return buffer_; }
  VkDeviceSize size() const { return size_; }

private:
  VkBuffer buffer_ = VK_NULL_HANDLE;
  VkDeviceMemory memory_ = VK_NULL_HANDLE;
  VkDeviceSize size_ = 0;
};

} // namespace gfx
