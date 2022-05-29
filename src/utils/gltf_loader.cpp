#include "utils/gltf_loader.h"

#include <d3d12.h>
#include <nlohmann/json.hpp>
#include <winrt/base.h>

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;

using nlohmann::json;

namespace utils {

static std::vector<uint8_t> LoadBinaryDataFromFile(fs::path path) {
  std::vector<uint8_t> data;

  std::ifstream strm(path.string(), std::ios::in | std::ios::binary | std::ios::ate);

  if (!strm.is_open())
    throw std::runtime_error("Could not open file: " + path.string());

  size_t fileSize = strm.tellg();
  data.resize(fileSize);

  strm.seekg(0, std::ios::beg);

  strm.read(reinterpret_cast<char*>(data.data()), fileSize);

  return data;
}

static ComponentType ParseComponentType(int type) {
  switch (type) {
    case 5123:
      return ComponentType::UnsignedShort;
    case 5126:
      return ComponentType::Float;
    default:
      throw std::invalid_argument("Invalid component type.");
  }
}

static AccessorType ParseAccessorType(const std::string& type) {
  if (type == "SCALAR")
     return AccessorType::Scalar;
  if (type == "VEC2")
    return AccessorType::Vec2;
  if (type == "VEC3")
    return AccessorType::Vec3;

  throw std::invalid_argument("Invalid accessor type.");
}

static int GetStrideFromAccessorType(AccessorType type) {
  switch (type) {
    case AccessorType::Scalar:
      return sizeof(float);
    case AccessorType::Vec2:
      return sizeof(float) * 2;
    case AccessorType::Vec3:
      return sizeof(float) * 3;
    default:
      throw std::invalid_argument("Invalid accessor type.");
  }
}

Scene LoadGltf(const char* path) {
  Scene scene{};

  fs::path gltfPath(path);

  std::ifstream strm(gltfPath.string());
  if (!strm.is_open())
    throw std::runtime_error("Could not open file: " + std::string(path));

  json gltfJson;
  strm >> gltfJson;

  for (auto& bufferJson : gltfJson["buffers"]) {
    fs::path binPath = gltfPath.parent_path() / bufferJson["uri"].get<std::string>();
    std::vector<uint8_t> data = LoadBinaryDataFromFile(binPath);

    scene.Buffers.push_back(std::move(data));
  }

  for (auto& bufferViewJson : gltfJson["bufferViews"]) {
    BufferView bufferView{};
    bufferView.BufferIndex = bufferViewJson["buffer"];
    bufferView.Length = bufferViewJson["byteLength"];
    bufferView.Offset = bufferViewJson["byteOffset"];

    scene.BufferViews.push_back(bufferView);
  }

  for (auto& accessorJson : gltfJson["accessors"]) {
    Accessor accessor{};
    accessor.BufferView = &scene.BufferViews[accessorJson["bufferView"]];
    accessor.ComponentType = ParseComponentType(accessorJson["componentType"]);
    accessor.Count = accessorJson["count"];
    accessor.Type = ParseAccessorType(accessorJson["type"]);

    if (!accessor.BufferView->Stride) {
      accessor.BufferView->Stride = GetStrideFromAccessorType(accessor.Type);
    }

    scene.Accessors.push_back(accessor);
  }

  if (gltfJson.contains("materials")) {
    for (auto& materialJson : gltfJson["materials"]) {
      json& roughnessJson = materialJson["pbrMetallicRoughness"];

      PbrMetallicRoughness roughness{};
      roughness.BaseColorFactor[0] = roughnessJson["baseColorFactor"][0];
      roughness.BaseColorFactor[1] = roughnessJson["baseColorFactor"][1];
      roughness.BaseColorFactor[2] = roughnessJson["baseColorFactor"][2];
      roughness.BaseColorFactor[3] = roughnessJson["baseColorFactor"][3];
      roughness.MetallicFactor = roughnessJson["metallicFactor"];
      roughness.RoughnessFactor = roughnessJson["roughnessFactor"];

      Material material{};
      material.PbrMetallicRoughness = roughness;

      scene.Materials.push_back(material);
    }
  }

  for (auto& meshJson : gltfJson["meshes"]) {
    Mesh mesh{};

    for (auto& primJson : meshJson["primitives"]) {
      Primitive prim{};

      int positionsIndex = primJson["attributes"]["POSITION"];
      int normalsIndex = primJson["attributes"]["NORMAL"];
      int indicesIndex = primJson["indices"];

      prim.Positions = &scene.Accessors[positionsIndex];
      prim.Normals = &scene.Accessors[normalsIndex];
      prim.Indices = &scene.Accessors[indicesIndex];

      if (primJson.contains("material")) {
        prim.MaterialIndex = primJson["material"];
      }

      mesh.Primitives.push_back(std::move(prim));
    }

    scene.Meshes.push_back(std::move(mesh));
  }

  return scene;
}

} // namespace utils
