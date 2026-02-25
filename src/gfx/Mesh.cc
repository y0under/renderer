#include "gfx/Mesh.h"

#include "gfx/Buffer.h"
#include "gfx/Context.h"

#include <cstddef>
#include <cstdint>
#include <new>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace gfx {

namespace {

[[noreturn]] void fail(char const* msg) { throw std::runtime_error(msg); }
[[noreturn]] void fail(std::string const& msg) { throw std::runtime_error(msg); }

} // namespace

struct Mesh::Impl {
  Buffer vb{};
  Buffer ib{};
};

Mesh::~Mesh() {
}

Mesh::Mesh(Mesh&& other) noexcept {
  *this = std::move(other);
}

Mesh& Mesh::operator=(Mesh&& other) noexcept {
  if (this == &other) {
    return *this;
  }

  impl_ = other.impl_;
  vertex_count_ = other.vertex_count_;
  index_count_ = other.index_count_;

  other.impl_ = nullptr;
  other.vertex_count_ = 0;
  other.index_count_ = 0;

  return *this;
}

void Mesh::init_quad(Context const& ctx) {
  if (impl_ != nullptr) {
    fail("Mesh::init_quad called twice");
  }

  impl_ = new (std::nothrow) Impl();
  if (impl_ == nullptr) {
    fail("Mesh: allocation failed");
  }

  // A colored quad on z=0 plane (two triangles).
  std::vector<Vertex> v{
    Vertex{{-0.6f, -0.4f, 0.0f}, {1.0f, 0.2f, 0.2f}}, // 0
    Vertex{{ 0.6f, -0.4f, 0.0f}, {0.2f, 1.0f, 0.2f}}, // 1
    Vertex{{ 0.6f,  0.4f, 0.0f}, {0.2f, 0.2f, 1.0f}}, // 2
    Vertex{{-0.6f,  0.4f, 0.0f}, {1.0f, 1.0f, 0.2f}}, // 3
  };

  std::vector<Index> idx{
    0u, 1u, 2u,
    2u, 3u, 0u,
  };

  vertex_count_ = static_cast<std::uint32_t>(v.size());
  index_count_ = static_cast<std::uint32_t>(idx.size());

  {
    VkDeviceSize const bytes = static_cast<VkDeviceSize>(v.size() * sizeof(Vertex));
    impl_->vb.init(ctx,
                   bytes,
                   VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    impl_->vb.upload(ctx, v.data(), v.size() * sizeof(Vertex));
  }

  {
    VkDeviceSize const bytes = static_cast<VkDeviceSize>(idx.size() * sizeof(Index));
    impl_->ib.init(ctx,
                   bytes,
                   VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    impl_->ib.upload(ctx, idx.data(), idx.size() * sizeof(Index));
  }
}

void Mesh::shutdown(Context const& ctx) {
  if (impl_ != nullptr) {
    impl_->ib.shutdown(ctx);
    impl_->vb.shutdown(ctx);
    delete impl_;
    impl_ = nullptr;
  }
  vertex_count_ = 0;
  index_count_ = 0;
}

VkBuffer Mesh::vertex_buffer() const {
  return (impl_ != nullptr) ? impl_->vb.handle() : VK_NULL_HANDLE;
}

VkBuffer Mesh::index_buffer() const {
  return (impl_ != nullptr) ? impl_->ib.handle() : VK_NULL_HANDLE;
}

} // namespace gfx
