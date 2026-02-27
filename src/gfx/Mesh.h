#pragma once

#include <vulkan/vulkan.h>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace gfx {

class Context;
class Buffer;
class Upload;

struct Vertex {
  float pos[3];
  float color[3];

  static VkVertexInputBindingDescription binding_description() {
    VkVertexInputBindingDescription b{};
    b.binding = 0;
    b.stride = static_cast<uint32_t>(sizeof(Vertex));
    b.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    return b;
  }

  static void attribute_descriptions(VkVertexInputAttributeDescription out[2]) {
    VkVertexInputAttributeDescription a0{};
    a0.location = 0;
    a0.binding = 0;
    a0.format = VK_FORMAT_R32G32B32_SFLOAT;
    a0.offset = static_cast<uint32_t>(offsetof(Vertex, pos));

    VkVertexInputAttributeDescription a1{};
    a1.location = 1;
    a1.binding = 0;
    a1.format = VK_FORMAT_R32G32B32_SFLOAT;
    a1.offset = static_cast<uint32_t>(offsetof(Vertex, color));

    out[0] = a0;
    out[1] = a1;
  }
};

class Mesh {
public:
  using Index = std::uint32_t;

public:
  Mesh() = default;
  ~Mesh();

  Mesh(Mesh const&) = delete;
  Mesh& operator=(Mesh const&) = delete;

  Mesh(Mesh&& other) noexcept;
  Mesh& operator=(Mesh&& other) noexcept;

  void init_from_data(Context const& ctx,
                      Upload& uploader,
                      std::vector<Vertex> const& vertices,
                      std::vector<Index> const& indices);

  void init_quad(Context const& ctx, Upload& uploader);
  void shutdown(Context const& ctx);

  VkBuffer vertex_buffer() const;
  VkBuffer index_buffer() const;

  std::uint32_t vertex_count() const { return vertex_count_; }
  std::uint32_t index_count() const { return index_count_; }
  VkIndexType index_type() const { return VK_INDEX_TYPE_UINT32; }

private:
  struct Impl;
  Impl* impl_ = nullptr;

  std::uint32_t vertex_count_ = 0;
  std::uint32_t index_count_ = 0;
};

} // namespace gfx
