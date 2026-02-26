#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>

namespace gfx {

class Context;

class Upload {
public:
  Upload() = default;
  ~Upload();

  Upload(Upload const&) = delete;
  Upload& operator=(Upload const&) = delete;

  Upload(Upload&& other) noexcept;
  Upload& operator=(Upload&& other) noexcept;

  void init(Context const& ctx);
  void shutdown(Context const& ctx);

  // Begin/End one-shot command buffer, submit to graphics queue, and wait for completion.
  VkCommandBuffer begin(Context const& ctx);
  void end_and_submit(Context const& ctx, VkCommandBuffer cb);

private:
  VkCommandPool command_pool_ = VK_NULL_HANDLE;
};

} // namespace gfx
