#pragma once

#include <vulkan/vulkan.h>

#include <string>

namespace gfx {

class Context;
class Swapchain;

class Pipeline {
public:
  Pipeline() = default;
  ~Pipeline();

  Pipeline(Pipeline const&) = delete;
  Pipeline& operator=(Pipeline const&) = delete;

  Pipeline(Pipeline&& other) noexcept;
  Pipeline& operator=(Pipeline&& other) noexcept;

  // vert/frag are SPIR-V paths.
  void init(Context const& ctx, Swapchain const& sc, std::string const& vert_spv_path, std::string const& frag_spv_path);
  void shutdown(Context const& ctx);

  VkRenderPass render_pass() const { return render_pass_; }
  VkPipeline pipeline() const { return pipeline_; }
  VkPipelineLayout pipeline_layout() const { return pipeline_layout_; }

private:
  VkShaderModule create_shader_module(Context const& ctx, std::string const& path);

private:
  VkRenderPass render_pass_ = VK_NULL_HANDLE;
  VkPipelineLayout pipeline_layout_ = VK_NULL_HANDLE;
  VkPipeline pipeline_ = VK_NULL_HANDLE;
};

} // namespace gfx
