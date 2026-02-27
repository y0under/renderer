#include "gfx/Mesh.h"

#include "gfx/Buffer.h"
#include "gfx/Context.h"
#include "gfx/Upload.h"

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

void Mesh::init_from_data(Context const& ctx,
                          Upload& uploader,
                          std::vector<Vertex> const& vertices,
                          std::vector<Index> const& indices) {
  if (impl_ != nullptr) {
    fail("Mesh::init_from_data called twice");
  }
  if (vertices.empty() || indices.empty()) {
    fail("Mesh::init_from_data: empty vertices/indices");
  }

  impl_ = new (std::nothrow) Impl();
  if (impl_ == nullptr) {
    fail("Mesh: allocation failed");
  }

  vertex_count_ = static_cast<std::uint32_t>(vertices.size());
  index_count_ = static_cast<std::uint32_t>(indices.size());

  impl_->vb.init_device_local_with_staging(ctx,
                                           uploader,
                                           vertices.data(),
                                           vertices.size() * sizeof(Vertex),
                                           VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

  impl_->ib.init_device_local_with_staging(ctx,
                                           uploader,
                                           indices.data(),
                                           indices.size() * sizeof(Index),
                                           VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
}

void Mesh::init_quad(Context const& ctx, Upload& uploader) {
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

  init_from_data(ctx, uploader, v, idx);
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
