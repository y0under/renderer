#include "gfx/Renderer.h"

#include "gfx/Context.h"
#include "gfx/Mesh.h"
#include "gfx/Pipeline.h"
#include "gfx/Swapchain.h"
#include "math/Camera.h"

#include <GLFW/glfw3.h>

#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <glm/glm.hpp>

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

Renderer::~Renderer() {
}

Renderer::Renderer(Renderer&& other) noexcept {
  *this = std::move(other);
}

Renderer& Renderer::operator=(Renderer&& other) noexcept {
  if (this == &other) {
    return *this;
  }

  command_pool_ = other.command_pool_;
  command_buffers_ = std::move(other.command_buffers_);
  framebuffers_ = std::move(other.framebuffers_);
  image_available_ = std::move(other.image_available_);
  render_finished_ = std::move(other.render_finished_);
  in_flight_ = std::move(other.in_flight_);
  frame_index_ = other.frame_index_;

  other.command_pool_ = VK_NULL_HANDLE;
  other.command_buffers_.clear();
  other.framebuffers_.clear();
  other.image_available_.clear();
  other.render_finished_.clear();
  other.in_flight_.clear();
  other.frame_index_ = 0;

  return *this;
}

void Renderer::init(Context const& ctx, Swapchain const& sc, Pipeline const& pl) {
  create_command_pool(ctx);
  allocate_command_buffers(ctx, sc.images().size());
  create_framebuffers(ctx, sc, pl);
  create_sync(ctx);
}

void Renderer::shutdown(Context const& ctx) {
  vk_check(vkDeviceWaitIdle(ctx.device()), "vkDeviceWaitIdle");

  destroy_sync(ctx);
  destroy_framebuffers(ctx);
  free_command_buffers(ctx);

  if (command_pool_ != VK_NULL_HANDLE) {
    vkDestroyCommandPool(ctx.device(), command_pool_, nullptr);
    command_pool_ = VK_NULL_HANDLE;
  }

  frame_index_ = 0;
}

void Renderer::create_command_pool(Context const& ctx) {
  VkCommandPoolCreateInfo ci{};
  ci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  ci.queueFamilyIndex = ctx.graphics_queue_family();
  ci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

  vk_check(vkCreateCommandPool(ctx.device(), &ci, nullptr, &command_pool_), "vkCreateCommandPool");
}

void Renderer::allocate_command_buffers(Context const& ctx, std::size_t count) {
  if (count == 0U) {
    fail("allocate_command_buffers: count == 0");
  }

  command_buffers_.assign(count, VK_NULL_HANDLE);

  VkCommandBufferAllocateInfo ai{};
  ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  ai.commandPool = command_pool_;
  ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  ai.commandBufferCount = static_cast<uint32_t>(count);

  vk_check(vkAllocateCommandBuffers(ctx.device(), &ai, command_buffers_.data()), "vkAllocateCommandBuffers");
}

void Renderer::free_command_buffers(Context const& ctx) {
  if (command_pool_ == VK_NULL_HANDLE || command_buffers_.empty()) {
    command_buffers_.clear();
    return;
  }

  vkFreeCommandBuffers(ctx.device(), command_pool_, static_cast<uint32_t>(command_buffers_.size()), command_buffers_.data());
  command_buffers_.clear();
}

void Renderer::create_sync(Context const& ctx) {
  image_available_.assign(kMaxFramesInFlight, VK_NULL_HANDLE);
  render_finished_.assign(kMaxFramesInFlight, VK_NULL_HANDLE);
  in_flight_.assign(kMaxFramesInFlight, VK_NULL_HANDLE);

  VkSemaphoreCreateInfo sci{};
  sci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

  VkFenceCreateInfo fci{};
  fci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fci.flags = VK_FENCE_CREATE_SIGNALED_BIT;

  for (std::uint32_t i = 0; i < kMaxFramesInFlight; ++i) {
    vk_check(vkCreateSemaphore(ctx.device(), &sci, nullptr, &image_available_[i]), "vkCreateSemaphore(image_available)");
    vk_check(vkCreateSemaphore(ctx.device(), &sci, nullptr, &render_finished_[i]), "vkCreateSemaphore(render_finished)");
    vk_check(vkCreateFence(ctx.device(), &fci, nullptr, &in_flight_[i]), "vkCreateFence(in_flight)");
  }
}

void Renderer::destroy_sync(Context const& ctx) {
  for (auto& f : in_flight_) {
    if (f != VK_NULL_HANDLE) {
      vkDestroyFence(ctx.device(), f, nullptr);
      f = VK_NULL_HANDLE;
    }
  }
  for (auto& s : render_finished_) {
    if (s != VK_NULL_HANDLE) {
      vkDestroySemaphore(ctx.device(), s, nullptr);
      s = VK_NULL_HANDLE;
    }
  }
  for (auto& s : image_available_) {
    if (s != VK_NULL_HANDLE) {
      vkDestroySemaphore(ctx.device(), s, nullptr);
      s = VK_NULL_HANDLE;
    }
  }
  in_flight_.clear();
  render_finished_.clear();
  image_available_.clear();
}

void Renderer::create_framebuffers(Context const& ctx, Swapchain const& sc, Pipeline const& pl) {
  framebuffers_.assign(sc.image_views().size(), VK_NULL_HANDLE);

  for (std::size_t i = 0; i < sc.image_views().size(); ++i) {
    VkImageView attachments[] = {sc.image_views()[i]};

    VkFramebufferCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    ci.renderPass = pl.render_pass();
    ci.attachmentCount = 1;
    ci.pAttachments = attachments;
    ci.width = sc.extent().width;
    ci.height = sc.extent().height;
    ci.layers = 1;

    vk_check(vkCreateFramebuffer(ctx.device(), &ci, nullptr, &framebuffers_[i]), "vkCreateFramebuffer");
  }
}

void Renderer::destroy_framebuffers(Context const& ctx) {
  for (auto& fb : framebuffers_) {
    if (fb != VK_NULL_HANDLE) {
      vkDestroyFramebuffer(ctx.device(), fb, nullptr);
      fb = VK_NULL_HANDLE;
    }
  }
  framebuffers_.clear();
}

void Renderer::record_command_buffer(VkCommandBuffer cb,
                                     Swapchain const& sc,
                                     Pipeline const& pl,
                                     VkFramebuffer fb,
                                     Mesh const& mesh,
                                     math::Camera const& cam) {
  VkCommandBufferBeginInfo bi{};
  bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  vk_check(vkBeginCommandBuffer(cb, &bi), "vkBeginCommandBuffer");

  VkClearValue clear{};
  clear.color.float32[0] = 0.05f;
  clear.color.float32[1] = 0.05f;
  clear.color.float32[2] = 0.10f;
  clear.color.float32[3] = 1.0f;

  VkRenderPassBeginInfo rpbi{};
  rpbi.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  rpbi.renderPass = pl.render_pass();
  rpbi.framebuffer = fb;
  rpbi.renderArea.offset = {0, 0};
  rpbi.renderArea.extent = sc.extent();
  rpbi.clearValueCount = 1;
  rpbi.pClearValues = &clear;

  vkCmdBeginRenderPass(cb, &rpbi, VK_SUBPASS_CONTENTS_INLINE);
  vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pl.pipeline());

  VkViewport viewport{};
  viewport.x = 0.0f;
  viewport.y = 0.0f;
  viewport.width = static_cast<float>(sc.extent().width);
  viewport.height = static_cast<float>(sc.extent().height);
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;
  vkCmdSetViewport(cb, 0, 1, &viewport);

  VkRect2D scissor{};
  scissor.offset = {0, 0};
  scissor.extent = sc.extent();
  vkCmdSetScissor(cb, 0, 1, &scissor);

  float const aspect =
    (sc.extent().height != 0U)
      ? (static_cast<float>(sc.extent().width) / static_cast<float>(sc.extent().height))
      : 1.0f;

  glm::mat4 const model(1.0f);
  glm::mat4 const mvp = cam.mvp(aspect, model);

  vkCmdPushConstants(cb, pl.pipeline_layout(), VK_SHADER_STAGE_VERTEX_BIT, 0, 16u * static_cast<uint32_t>(sizeof(float)), &mvp);

  VkBuffer vb = mesh.vertex_buffer();
  VkDeviceSize offsets[] = {0};
  vkCmdBindVertexBuffers(cb, 0, 1, &vb, offsets);

  vkCmdDraw(cb, mesh.vertex_count(), 1, 0, 0);

  vkCmdEndRenderPass(cb);
  vk_check(vkEndCommandBuffer(cb), "vkEndCommandBuffer");
}

void Renderer::recreate_swapchain_dependent(Context const& ctx, GLFWwindow* window, Swapchain& sc, Pipeline const& pl) {
  vk_check(vkDeviceWaitIdle(ctx.device()), "vkDeviceWaitIdle");

  sc.recreate(ctx, window);

  destroy_framebuffers(ctx);

  std::size_t const image_count = sc.images().size();
  if (command_buffers_.size() != image_count) {
    free_command_buffers(ctx);
    allocate_command_buffers(ctx, image_count);
  }

  create_framebuffers(ctx, sc, pl);
}

bool Renderer::draw_frame(Context const& ctx,
                          GLFWwindow* window,
                          Swapchain& sc,
                          Pipeline const& pl,
                          Mesh const& mesh,
                          math::Camera const& cam) {
  if (window == nullptr) {
    fail("Renderer::draw_frame: window == nullptr");
  }

  VkFence const fence = in_flight_.at(frame_index_);
  vk_check(vkWaitForFences(ctx.device(), 1, &fence, VK_TRUE, UINT64_MAX), "vkWaitForFences");
  vk_check(vkResetFences(ctx.device(), 1, &fence), "vkResetFences");

  uint32_t image_index = 0;
  VkResult acq = vkAcquireNextImageKHR(
    ctx.device(),
    sc.handle(),
    UINT64_MAX,
    image_available_.at(frame_index_),
    VK_NULL_HANDLE,
    &image_index
  );

  if (acq == VK_ERROR_OUT_OF_DATE_KHR) {
    recreate_swapchain_dependent(ctx, window, sc, pl);
    return false;
  }

  if (acq != VK_SUCCESS && acq != VK_SUBOPTIMAL_KHR) {
    fail(std::string("vkAcquireNextImageKHR failed: ") + std::to_string(static_cast<int>(acq)));
  }

  VkCommandBuffer cb = command_buffers_.at(static_cast<std::size_t>(image_index));
  vk_check(vkResetCommandBuffer(cb, 0), "vkResetCommandBuffer");
  record_command_buffer(cb, sc, pl, framebuffers_.at(static_cast<std::size_t>(image_index)), mesh, cam);

  VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

  VkSubmitInfo si{};
  si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  si.waitSemaphoreCount = 1;
  si.pWaitSemaphores = &image_available_.at(frame_index_);
  si.pWaitDstStageMask = &wait_stage;
  si.commandBufferCount = 1;
  si.pCommandBuffers = &cb;
  si.signalSemaphoreCount = 1;
  si.pSignalSemaphores = &render_finished_.at(frame_index_);

  vk_check(vkQueueSubmit(ctx.graphics_queue(), 1, &si, fence), "vkQueueSubmit");

  VkPresentInfoKHR pi{};
  pi.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  pi.waitSemaphoreCount = 1;
  pi.pWaitSemaphores = &render_finished_.at(frame_index_);
  VkSwapchainKHR sc_handle = sc.handle();
  pi.swapchainCount = 1;
  pi.pSwapchains = &sc_handle;
  pi.pImageIndices = &image_index;

  VkResult pres = vkQueuePresentKHR(ctx.present_queue(), &pi);
  if (pres == VK_ERROR_OUT_OF_DATE_KHR || pres == VK_SUBOPTIMAL_KHR || acq == VK_SUBOPTIMAL_KHR) {
    recreate_swapchain_dependent(ctx, window, sc, pl);
    frame_index_ = (frame_index_ + 1U) % kMaxFramesInFlight;
    return false;
  }

  if (pres != VK_SUCCESS) {
    fail(std::string("vkQueuePresentKHR failed: ") + std::to_string(static_cast<int>(pres)));
  }

  frame_index_ = (frame_index_ + 1U) % kMaxFramesInFlight;
  return true;
}

} // namespace gfx
