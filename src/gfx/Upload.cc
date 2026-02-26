#include "gfx/Upload.h"

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

} // namespace

Upload::~Upload() {
}

Upload::Upload(Upload&& other) noexcept {
  *this = std::move(other);
}

Upload& Upload::operator=(Upload&& other) noexcept {
  if (this == &other) {
    return *this;
  }

  command_pool_ = other.command_pool_;
  other.command_pool_ = VK_NULL_HANDLE;
  return *this;
}

void Upload::init(Context const& ctx) {
  if (command_pool_ != VK_NULL_HANDLE) {
    fail("Upload::init called twice");
  }

  VkCommandPoolCreateInfo ci{};
  ci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  ci.queueFamilyIndex = ctx.graphics_queue_family();
  ci.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;

  vk_check(vkCreateCommandPool(ctx.device(), &ci, nullptr, &command_pool_), "vkCreateCommandPool(Upload)");
}

void Upload::shutdown(Context const& ctx) {
  if (command_pool_ != VK_NULL_HANDLE) {
    vkDestroyCommandPool(ctx.device(), command_pool_, nullptr);
    command_pool_ = VK_NULL_HANDLE;
  }
}

VkCommandBuffer Upload::begin(Context const& ctx) {
  if (command_pool_ == VK_NULL_HANDLE) {
    fail("Upload::begin: not initialized");
  }

  VkCommandBuffer cb = VK_NULL_HANDLE;

  VkCommandBufferAllocateInfo ai{};
  ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  ai.commandPool = command_pool_;
  ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  ai.commandBufferCount = 1;

  vk_check(vkAllocateCommandBuffers(ctx.device(), &ai, &cb), "vkAllocateCommandBuffers(Upload)");

  VkCommandBufferBeginInfo bi{};
  bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

  vk_check(vkBeginCommandBuffer(cb, &bi), "vkBeginCommandBuffer(Upload)");
  return cb;
}

void Upload::end_and_submit(Context const& ctx, VkCommandBuffer cb) {
  if (cb == VK_NULL_HANDLE) {
    fail("Upload::end_and_submit: cb == null");
  }

  vk_check(vkEndCommandBuffer(cb), "vkEndCommandBuffer(Upload)");

  VkSubmitInfo si{};
  si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  si.commandBufferCount = 1;
  si.pCommandBuffers = &cb;

  vk_check(vkQueueSubmit(ctx.graphics_queue(), 1, &si, VK_NULL_HANDLE), "vkQueueSubmit(Upload)");
  vk_check(vkQueueWaitIdle(ctx.graphics_queue()), "vkQueueWaitIdle(Upload)");

  vkFreeCommandBuffers(ctx.device(), command_pool_, 1, &cb);
}

} // namespace gfx
