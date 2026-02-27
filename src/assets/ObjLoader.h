#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace assets {

struct ObjMesh {
  std::vector<float> positions_xyz;     // flat array: x,y,z,...
  std::vector<std::uint32_t> indices;   // triangulated
};

class ObjLoader {
public:
  // Supports:
  //  - v x y z
  //  - f a b c [d ...] where a/b/c may be "v", "v/vt", "v//vn", "v/vt/vn"
  // Ignores vt/vn, triangulates n-gons by fan.
  static ObjMesh load(std::string const& path);

private:
  static std::int32_t parse_vertex_index_token(std::string const& token);
  static std::int32_t to_zero_based_index(std::int32_t obj_index, std::size_t vertex_count);
};

} // namespace assets
