#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>
#include <vector>

struct GLFWwindow;

namespace gfx {

class Context;
class Swapchain;
class Pipeline;

class Renderer {
public:
  Renderer() = default;
  ~Renderer();

  Renderer(Renderer const&) = delete;
  Renderer& operator=(Renderer const&) = delete;

  Renderer(Renderer&& other) noexcept;
  Renderer& operator=(Renderer&& other) noexcept;

  void init(Context const& ctx, Swapchain const& sc, Pipeline const& pl);
  void shutdown(Context const& ctx);

  // returns false when swapchain was recreated (caller may want to react)
  bool draw_frame(Context const& ctx, GLFWwindow* window, Swapchain& sc, Pipeline& pl);

private:
  void create_command_pool(Context const& ctx);
  void allocate_command_buffers(Context const& ctx, std::size_t count);
  void create_sync(Context const& ctx);
  void destroy_sync(Context const& ctx);

  void create_framebuffers(Context const& ctx, Swapchain const& sc, Pipeline const& pl);
  void destroy_framebuffers(Context const& ctx);

  void record_command_buffer(VkCommandBuffer cb, Swapchain const& sc, Pipeline const& pl, VkFramebuffer fb);

private:
  static constexpr std::uint32_t kMaxFramesInFlight = 2;

  VkCommandPool command_pool_ = VK_NULL_HANDLE;
  std::vector<VkCommandBuffer> command_buffers_;

  std::vector<VkFramebuffer> framebuffers_;

  std::vector<VkSemaphore> image_available_;
  std::vector<VkSemaphore> render_finished_;
  std::vector<VkFence> in_flight_;

  std::uint32_t frame_index_ = 0;
};

} // namespace gfx
