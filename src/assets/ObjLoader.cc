#include "assets/ObjLoader.h"

#include <cctype>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace assets {

namespace {

[[noreturn]] void fail(std::string const& msg) {
  throw std::runtime_error(msg);
}

bool starts_with(std::string const& s, char const* prefix) {
  std::size_t i = 0;
  while (prefix[i] != '\0') {
    if (i >= s.size() || s[i] != prefix[i]) {
      return false;
    }
    ++i;
  }
  return true;
}

} // namespace

std::int32_t ObjLoader::parse_vertex_index_token(std::string const& token) {
  // token formats: "v", "v/vt", "v//vn", "v/vt/vn"
  // We only need the leading signed integer v.
  std::size_t const slash = token.find('/');
  std::string const head = (slash == std::string::npos) ? token : token.substr(0, slash);

  if (head.empty()) {
    fail("OBJ: face token has empty vertex index");
  }

  std::size_t pos = 0;
  bool neg = false;
  if (head[pos] == '-') {
    neg = true;
    ++pos;
  }

  if (pos >= head.size()) {
    fail("OBJ: invalid vertex index token: " + token);
  }
  if (!std::isdigit(static_cast<unsigned char>(head[pos]))) {
    fail("OBJ: invalid vertex index token: " + token);
  }

  std::int32_t value = 0;
  for (; pos < head.size(); ++pos) {
    char const c = head[pos];
    if (!std::isdigit(static_cast<unsigned char>(c))) {
      fail("OBJ: invalid vertex index token: " + token);
    }
    value = static_cast<std::int32_t>(value * 10 + (c - '0'));
  }

  return neg ? -value : value;
}

std::int32_t ObjLoader::to_zero_based_index(std::int32_t obj_index, std::size_t vertex_count) {
  // OBJ: 1-based positive, negative means relative to end: -1 is last.
  if (obj_index > 0) {
    std::int32_t const z = obj_index - 1;
    if (z < 0 || static_cast<std::size_t>(z) >= vertex_count) {
      fail("OBJ: vertex index out of range");
    }
    return z;
  }

  if (obj_index < 0) {
    std::int64_t const z = static_cast<std::int64_t>(vertex_count) + static_cast<std::int64_t>(obj_index);
    if (z < 0 || z >= static_cast<std::int64_t>(vertex_count)) {
      fail("OBJ: negative vertex index out of range");
    }
    return static_cast<std::int32_t>(z);
  }

  fail("OBJ: vertex index 0 is invalid");
}

ObjMesh ObjLoader::load(std::string const& path) {
  std::ifstream ifs(path);
  if (!ifs) {
    fail("OBJ: failed to open: " + path);
  }

  ObjMesh out{};
  std::string line;

  std::size_t line_no = 0;
  while (std::getline(ifs, line)) {
    ++line_no;

    if (line.empty()) {
      continue;
    }

    // Skip comments
    if (!line.empty() && line[0] == '#') {
      continue;
    }

    // Trim leading spaces
    std::size_t first = 0;
    while (first < line.size() && std::isspace(static_cast<unsigned char>(line[first])) != 0) {
      ++first;
    }
    if (first >= line.size()) {
      continue;
    }

    std::string const s = line.substr(first);
    if (starts_with(s, "v ")) {
      std::istringstream iss(s);
      char vtag = '\0';
      float x = 0.0f;
      float y = 0.0f;
      float z = 0.0f;

      iss >> vtag >> x >> y >> z;
      if (!iss) {
        fail("OBJ: malformed vertex at line " + std::to_string(line_no));
      }
      out.positions_xyz.push_back(x);
      out.positions_xyz.push_back(y);
      out.positions_xyz.push_back(z);
      continue;
    }

    if (starts_with(s, "f ")) {
      std::istringstream iss(s);
      char ftag = '\0';
      iss >> ftag;

      std::vector<std::int32_t> face;
      std::string tok;
      while (iss >> tok) {
        std::int32_t const vi = parse_vertex_index_token(tok);
        face.push_back(vi);
      }

      if (face.size() < 3U) {
        fail("OBJ: face has <3 vertices at line " + std::to_string(line_no));
      }

      std::size_t const vcount = out.positions_xyz.size() / 3U;

      // triangulate fan: (0, i, i+1)
      std::int32_t const i0 = to_zero_based_index(face[0], vcount);

      for (std::size_t i = 1; i + 1 < face.size(); ++i) {
        std::int32_t const i1 = to_zero_based_index(face[i], vcount);
        std::int32_t const i2 = to_zero_based_index(face[i + 1], vcount);

        out.indices.push_back(static_cast<std::uint32_t>(i0));
        out.indices.push_back(static_cast<std::uint32_t>(i1));
        out.indices.push_back(static_cast<std::uint32_t>(i2));
      }

      continue;
    }

    // Ignore other lines (vt, vn, usemtl, mtllib, o, g, s, etc.)
  }

  if (out.positions_xyz.empty()) {
    fail("OBJ: no vertices loaded: " + path);
  }
  if (out.indices.empty()) {
    fail("OBJ: no faces loaded (indices empty): " + path);
  }

  return out;
}

} // namespace assets
