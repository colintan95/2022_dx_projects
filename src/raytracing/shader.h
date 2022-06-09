#pragma once

#ifdef HLSL
typedef float4 XMFLOAT4A;
typedef float4x4 XMFLOAT4X4A;
#else
#include <DirectXMath.h>
using XMFLOAT4A = DirectX::XMFLOAT4A;
using XMFLOAT4X4A = DirectX::XMFLOAT4X4A;
#endif

struct Material {
  XMFLOAT4A BaseColor;
  float Metallic;
  float Roughness;
};

struct ClosestHitConstants {
  uint32_t NormalBufferStride;
};

namespace shader {

struct Quad {
  XMFLOAT4X4A BlasToAabb;
  float Width;
  float Height;
};

} // namespace shader
