#pragma once

#ifdef HLSL
typedef float4 XMFLOAT4A;
#else
#include <DirectXMath.h>
using XMFLOAT4A = DirectX::XMFLOAT4A;
#endif

struct Roughness {
  XMFLOAT4A BaseColorFactor;
  float MetallicFactor;
  float RoughnessFactor;
};

struct Material {
  Roughness Roughness;
};

struct ClosestHitConstants {
  uint32_t NormalBufferStride;
};
