#pragma once

#include <cstdint>
#include <optional>
#include <vector>

namespace utils {

struct BufferView {
  int BufferIndex;
  int Length;
  int Offset;
  std::optional<int> Stride;
};

enum class ComponentType {
  UnsignedShort,
  Float
};

enum class AccessorType {
  Scalar,
  Vec2,
  Vec3
};

struct Accessor {
  BufferView* BufferView;
  ComponentType ComponentType;
  int Count;
  AccessorType Type;
};

struct Primitive {
  Accessor* Positions;
  Accessor* Normals;
  Accessor* Indices;
};

struct Mesh {
  std::vector<Primitive> Primitives;
};

struct Scene {
  std::vector<std::vector<uint8_t>> Buffers;
  std::vector<BufferView> BufferViews;
  std::vector<Accessor> Accessors;

  std::vector<Mesh> Meshes;
};

Scene LoadGltf(const char* path);

} // namespace utils
