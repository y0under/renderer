#include "assets/ObjLoader.h"

#include <cctype>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
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

std::string ltrim(std::string const& line) {
  std::size_t i = 0;
  while (i < line.size() && std::isspace(static_cast<unsigned char>(line[i])) != 0) {
    ++i;
  }
  return (i < line.size()) ? line.substr(i) : std::string{};
}

struct Key {
  std::int32_t v;
  std::int32_t vt;
  std::int32_t vn;

  bool operator==(Key const& o) const {
    return v == o.v && vt == o.vt && vn == o.vn;
  }
};

struct KeyHash {
  std::size_t operator()(Key const& k) const noexcept {
    // cheap mix
    std::size_t h = static_cast<std::size_t>(k.v) * 1315423911u;
    h ^= static_cast<std::size_t>(k.vt) + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
    h ^= static_cast<std::size_t>(k.vn) + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
    return h;
  }
};

} // namespace

ObjLoader::Ref ObjLoader::parse_face_ref(std::string const& token) {
  // token: v, v/vt, v//vn, v/vt/vn
  Ref r{};

  std::size_t const s1 = token.find('/');
  if (s1 == std::string::npos) {
    // "v"
    r.v = std::stoi(token);
    return r;
  }

  std::string const a = token.substr(0, s1);
  r.v = a.empty() ? 0 : std::stoi(a);

  std::size_t const s2 = token.find('/', s1 + 1);
  if (s2 == std::string::npos) {
    // "v/vt"
    std::string const b = token.substr(s1 + 1);
    r.vt = b.empty() ? 0 : std::stoi(b);
    return r;
  }

  // "v/..../vn" (maybe empty middle)
  std::string const b = token.substr(s1 + 1, s2 - (s1 + 1));
  std::string const c = token.substr(s2 + 1);

  r.vt = b.empty() ? 0 : std::stoi(b);
  r.vn = c.empty() ? 0 : std::stoi(c);
  return r;
}

std::int32_t ObjLoader::to_zero_based(std::int32_t obj_index, std::size_t count, char const* what) {
  // OBJ: 1-based positive, negative means relative to end, 0 is invalid/missing (caller handles missing)
  if (obj_index > 0) {
    std::int32_t const z = obj_index - 1;
    if (z < 0 || static_cast<std::size_t>(z) >= count) {
      fail(std::string("OBJ: ") + what + " index out of range");
    }
    return z;
  }

  if (obj_index < 0) {
    std::int64_t const z = static_cast<std::int64_t>(count) + static_cast<std::int64_t>(obj_index);
    if (z < 0 || z >= static_cast<std::int64_t>(count)) {
      fail(std::string("OBJ: ") + what + " negative index out of range");
    }
    return static_cast<std::int32_t>(z);
  }

  fail(std::string("OBJ: ") + what + " index 0 is invalid");
}

ObjIndexedMesh ObjLoader::load(std::string const& path) {
  std::ifstream ifs(path);
  if (!ifs) {
    fail("OBJ: failed to open: " + path);
  }

  std::vector<float> positions;  // x,y,z...
  std::vector<float> texcoords;  // u,v...
  std::vector<float> normals;    // x,y,z...

  ObjIndexedMesh out{};

  std::unordered_map<Key, std::uint32_t, KeyHash> dedup;
  dedup.reserve(4096);

  std::string line;
  std::size_t line_no = 0;

  while (std::getline(ifs, line)) {
    ++line_no;

    std::string const s = ltrim(line);
    if (s.empty() || s[0] == '#') {
      continue;
    }

    if (starts_with(s, "v ")) {
      std::istringstream iss(s);
      char tag = '\0';
      float x = 0.0f;
      float y = 0.0f;
      float z = 0.0f;

      iss >> tag >> x >> y >> z;
      if (!iss) {
        fail("OBJ: malformed v at line " + std::to_string(line_no));
      }

      positions.push_back(x);
      positions.push_back(y);
      positions.push_back(z);
      continue;
    }

    if (starts_with(s, "vt ")) {
      std::istringstream iss(s);
      char t0 = '\0';
      char t1 = '\0';
      float u = 0.0f;
      float v = 0.0f;

      iss >> t0 >> t1 >> u >> v; // reads "v" "t" u v
      if (!iss) {
        fail("OBJ: malformed vt at line " + std::to_string(line_no));
      }

      texcoords.push_back(u);
      texcoords.push_back(v);
      continue;
    }

    if (starts_with(s, "vn ")) {
      std::istringstream iss(s);
      char t0 = '\0';
      char t1 = '\0';
      float x = 0.0f;
      float y = 0.0f;
      float z = 0.0f;

      iss >> t0 >> t1 >> x >> y >> z; // reads "v" "n" x y z
      if (!iss) {
        fail("OBJ: malformed vn at line " + std::to_string(line_no));
      }

      normals.push_back(x);
      normals.push_back(y);
      normals.push_back(z);
      continue;
    }

    if (starts_with(s, "f ")) {
      std::istringstream iss(s);
      char ftag = '\0';
      iss >> ftag;

      std::vector<Ref> face;
      std::string tok;
      while (iss >> tok) {
        face.push_back(parse_face_ref(tok));
      }

      if (face.size() < 3U) {
        fail("OBJ: face has <3 vertices at line " + std::to_string(line_no));
      }

      std::size_t const pos_count = positions.size() / 3U;
      std::size_t const uv_count = texcoords.size() / 2U;
      std::size_t const n_count = normals.size() / 3U;

      auto get_or_add = [&](Ref const& r) -> std::uint32_t {
        if (r.v == 0) {
          fail("OBJ: face vertex missing position index at line " + std::to_string(line_no));
        }

        std::int32_t const zv = to_zero_based(r.v, pos_count, "v");
        std::int32_t const zt = (r.vt != 0) ? to_zero_based(r.vt, uv_count, "vt") : -1;
        std::int32_t const zn = (r.vn != 0) ? to_zero_based(r.vn, n_count, "vn") : -1;

        Key const key{zv, zt, zn};
        auto it = dedup.find(key);
        if (it != dedup.end()) {
          return it->second;
        }

        VertexPNUV vtx{};

        vtx.pos[0] = positions[static_cast<std::size_t>(zv) * 3 + 0];
        vtx.pos[1] = positions[static_cast<std::size_t>(zv) * 3 + 1];
        vtx.pos[2] = positions[static_cast<std::size_t>(zv) * 3 + 2];

        if (zt >= 0) {
          vtx.uv[0] = texcoords[static_cast<std::size_t>(zt) * 2 + 0];
          vtx.uv[1] = texcoords[static_cast<std::size_t>(zt) * 2 + 1];
        } else {
          vtx.uv[0] = 0.0f;
          vtx.uv[1] = 0.0f;
        }

        if (zn >= 0) {
          vtx.normal[0] = normals[static_cast<std::size_t>(zn) * 3 + 0];
          vtx.normal[1] = normals[static_cast<std::size_t>(zn) * 3 + 1];
          vtx.normal[2] = normals[static_cast<std::size_t>(zn) * 3 + 2];
        } else {
          // fallback (no normals in file)
          vtx.normal[0] = 0.0f;
          vtx.normal[1] = 0.0f;
          vtx.normal[2] = 1.0f;
        }

        std::uint32_t const new_index = static_cast<std::uint32_t>(out.vertices.size());
        out.vertices.push_back(vtx);
        dedup.emplace(key, new_index);
        return new_index;
      };

      // triangulate fan: (0, i, i+1)
      std::uint32_t const i0 = get_or_add(face[0]);
      for (std::size_t i = 1; i + 1 < face.size(); ++i) {
        std::uint32_t const i1 = get_or_add(face[i]);
        std::uint32_t const i2 = get_or_add(face[i + 1]);
        out.indices.push_back(i0);
        out.indices.push_back(i1);
        out.indices.push_back(i2);
      }

      continue;
    }

    // Ignore: o, g, s, usemtl, mtllib, etc.
  }

  if (out.vertices.empty()) {
    fail("OBJ: no vertices generated: " + path);
  }
  if (out.indices.empty()) {
    fail("OBJ: no faces/indices generated: " + path);
  }

  return out;
}

} // namespace assets
