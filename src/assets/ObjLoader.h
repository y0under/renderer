#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace assets {

struct VertexPNUV {
  float pos[3];
  float normal[3];
  float uv[2];
};

struct ObjIndexedMesh {
  std::vector<VertexPNUV> vertices;     // deduplicated (pos, uv, normal)
  std::vector<std::uint32_t> indices;   // triangulated indices into vertices
};

class ObjLoader {
public:
  // Supports:
  //  - v x y z
  //  - vt u v [w]  (w ignored)
  //  - vn x y z
  //  - f a b c [d ...] where a/b/c may be:
  //      v
  //      v/vt
  //      v//vn
  //      v/vt/vn
  // Triangulates n-gons by fan and deduplicates by (v,vt,vn) tuple.
  static ObjIndexedMesh load(std::string const& path);

private:
  struct Ref {
    std::int32_t v = 0;   // OBJ index (can be negative), 0 means missing
    std::int32_t vt = 0;  // OBJ index (can be negative), 0 means missing
    std::int32_t vn = 0;  // OBJ index (can be negative), 0 means missing
  };

  static Ref parse_face_ref(std::string const& token);

  static std::int32_t to_zero_based(std::int32_t obj_index, std::size_t count, char const* what);
};

} // namespace assets
