#include "gfx/Pipeline.h"

#include "gfx/Context.h"
#include "gfx/Mesh.h"
#include "gfx/Swapchain.h"

#include <cstddef>
#include <cstdint>
#include <fstream>
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

std::vector<std::uint32_t> read_spirv(std::string const& path) {
  std::ifstream ifs(path, std::ios::binary | std::ios::ate);
  if (!ifs) {
    fail(std::string("Failed to open SPIR-V file: ") + path);
  }

  std::streamsize const size = ifs.tellg();
  if (size <= 0) {
    fail(std::string("SPIR-V file is empty: ") + path);
  }
  if ((size % 4) != 0) {
    fail(std::string("SPIR-V file size is not multiple of 4: ") + path);
  }

  std::vector<std::uint32_t> data(static_cast<std::size_t>(size / 4));
  ifs.seekg(0, std::ios::beg);
  if (!ifs.read(reinterpret_cast<char*>(data.data()), size)) {
    fail(std::string("Failed to read SPIR-V file: ") + path);
  }
  return data;
}

} // namespace

Pipeline::~Pipeline() {
}

Pipeline::Pipeline(Pipeline&& other) noexcept {
  *this = std::move(other);
}

Pipeline& Pipeline::operator=(Pipeline&& other) noexcept {
  if (this == &other) {
    return *this;
  }

  render_pass_ = other.render_pass_;
  pipeline_layout_ = other.pipeline_layout_;
  pipeline_ = other.pipeline_;

  other.render_pass_ = VK_NULL_HANDLE;
  other.pipeline_layout_ = VK_NULL_HANDLE;
  other.pipeline_ = VK_NULL_HANDLE;

  return *this;
}

void Pipeline::init(Context const& ctx,
                    Swapchain const& sc,
                    VkFormat depth_format,
                    std::string const& vert_spv_path,
                    std::string const& frag_spv_path) {
  // ---- Render pass (color + depth) ----
  VkAttachmentDescription attachments[2]{};

  attachments[0].format = sc.image_format();
  attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
  attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

  attachments[1].format = depth_format;
  attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
  attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  VkAttachmentReference color_ref{};
  color_ref.attachment = 0;
  color_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkAttachmentReference depth_ref{};
  depth_ref.attachment = 1;
  depth_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  VkSubpassDescription subpass{};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments = &color_ref;
  subpass.pDepthStencilAttachment = &depth_ref;

  VkSubpassDependency dep{};
  dep.srcSubpass = VK_SUBPASS_EXTERNAL;
  dep.dstSubpass = 0;
  dep.srcStageMask =
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
    | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
  dep.dstStageMask =
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
    | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
  dep.srcAccessMask = 0;
  dep.dstAccessMask =
      VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
    | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

  VkRenderPassCreateInfo rpci{};
  rpci.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  rpci.attachmentCount = 2;
  rpci.pAttachments = attachments;
  rpci.subpassCount = 1;
  rpci.pSubpasses = &subpass;
  rpci.dependencyCount = 1;
  rpci.pDependencies = &dep;

  vk_check(vkCreateRenderPass(ctx.device(), &rpci, nullptr, &render_pass_), "vkCreateRenderPass");

  // ---- Shader modules ----
  VkShaderModule vert = create_shader_module(ctx, vert_spv_path);
  VkShaderModule frag = create_shader_module(ctx, frag_spv_path);

  VkPipelineShaderStageCreateInfo stages[2]{};
  stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
  stages[0].module = vert;
  stages[0].pName = "main";

  stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  stages[1].module = frag;
  stages[1].pName = "main";

  // ---- Vertex input ----
  VkVertexInputBindingDescription binding = Vertex::binding_description();
  VkVertexInputAttributeDescription attrs[2]{};
  Vertex::attribute_descriptions(attrs);

  VkPipelineVertexInputStateCreateInfo vi{};
  vi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vi.vertexBindingDescriptionCount = 1;
  vi.pVertexBindingDescriptions = &binding;
  vi.vertexAttributeDescriptionCount = 2;
  vi.pVertexAttributeDescriptions = attrs;

  VkPipelineInputAssemblyStateCreateInfo ia{};
  ia.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  ia.primitiveRestartEnable = VK_FALSE;

  // ---- Dynamic viewport/scissor ----
  VkPipelineViewportStateCreateInfo vp{};
  vp.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  vp.viewportCount = 1;
  vp.scissorCount = 1;

  VkPipelineRasterizationStateCreateInfo rs{};
  rs.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rs.depthClampEnable = VK_FALSE;
  rs.rasterizerDiscardEnable = VK_FALSE;
  rs.polygonMode = VK_POLYGON_MODE_FILL;
  rs.cullMode = VK_CULL_MODE_BACK_BIT;
  rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
  rs.depthBiasEnable = VK_FALSE;
  rs.lineWidth = 1.0f;

  VkPipelineMultisampleStateCreateInfo ms{};
  ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

  VkPipelineDepthStencilStateCreateInfo ds_depth{};
  ds_depth.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  ds_depth.depthTestEnable = VK_TRUE;
  ds_depth.depthWriteEnable = VK_TRUE;
  ds_depth.depthCompareOp = VK_COMPARE_OP_LESS;
  ds_depth.depthBoundsTestEnable = VK_FALSE;
  ds_depth.stencilTestEnable = VK_FALSE;

  VkPipelineColorBlendAttachmentState cb_att{};
  cb_att.colorWriteMask =
      VK_COLOR_COMPONENT_R_BIT
    | VK_COLOR_COMPONENT_G_BIT
    | VK_COLOR_COMPONENT_B_BIT
    | VK_COLOR_COMPONENT_A_BIT;
  cb_att.blendEnable = VK_FALSE;

  VkPipelineColorBlendStateCreateInfo cb{};
  cb.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  cb.attachmentCount = 1;
  cb.pAttachments = &cb_att;

  VkDynamicState dynamics[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};

  VkPipelineDynamicStateCreateInfo ds{};
  ds.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  ds.dynamicStateCount = static_cast<uint32_t>(sizeof(dynamics) / sizeof(dynamics[0]));
  ds.pDynamicStates = dynamics;

  // ---- Push constant: mat4 mvp (64 bytes) ----
  VkPushConstantRange pcr{};
  pcr.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
  pcr.offset = 0;
  pcr.size = 16u * static_cast<uint32_t>(sizeof(float));

  VkPipelineLayoutCreateInfo plci{};
  plci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  plci.pushConstantRangeCount = 1;
  plci.pPushConstantRanges = &pcr;

  vk_check(vkCreatePipelineLayout(ctx.device(), &plci, nullptr, &pipeline_layout_), "vkCreatePipelineLayout");

  VkGraphicsPipelineCreateInfo gpci{};
  gpci.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  gpci.stageCount = 2;
  gpci.pStages = stages;
  gpci.pVertexInputState = &vi;
  gpci.pInputAssemblyState = &ia;
  gpci.pViewportState = &vp;
  gpci.pRasterizationState = &rs;
  gpci.pMultisampleState = &ms;
  gpci.pDepthStencilState = &ds_depth;
  gpci.pColorBlendState = &cb;
  gpci.pDynamicState = &ds;
  gpci.layout = pipeline_layout_;
  gpci.renderPass = render_pass_;
  gpci.subpass = 0;

  vk_check(vkCreateGraphicsPipelines(ctx.device(), VK_NULL_HANDLE, 1, &gpci, nullptr, &pipeline_),
           "vkCreateGraphicsPipelines");

  vkDestroyShaderModule(ctx.device(), frag, nullptr);
  vkDestroyShaderModule(ctx.device(), vert, nullptr);
}

void Pipeline::shutdown(Context const& ctx) {
  if (pipeline_ != VK_NULL_HANDLE) {
    vkDestroyPipeline(ctx.device(), pipeline_, nullptr);
    pipeline_ = VK_NULL_HANDLE;
  }
  if (pipeline_layout_ != VK_NULL_HANDLE) {
    vkDestroyPipelineLayout(ctx.device(), pipeline_layout_, nullptr);
    pipeline_layout_ = VK_NULL_HANDLE;
  }
  if (render_pass_ != VK_NULL_HANDLE) {
    vkDestroyRenderPass(ctx.device(), render_pass_, nullptr);
    render_pass_ = VK_NULL_HANDLE;
  }
}

VkShaderModule Pipeline::create_shader_module(Context const& ctx, std::string const& path) {
  auto const code = read_spirv(path);

  VkShaderModuleCreateInfo ci{};
  ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  ci.codeSize = code.size() * sizeof(std::uint32_t);
  ci.pCode = code.data();

  VkShaderModule m = VK_NULL_HANDLE;
  vk_check(vkCreateShaderModule(ctx.device(), &ci, nullptr, &m), "vkCreateShaderModule");
  return m;
}

} // namespace gfx
