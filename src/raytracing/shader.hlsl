#include "shader.h"

// Global descriptors.

RaytracingAccelerationStructure s_scene : register(t0);
RWTexture2D<float4> s_film : register(u0);

// Closest hit descriptors.

struct ClosestHitMatrices {
  float3x4 Transform;
};

ByteAddressBuffer s_normalBuffer : register(t1);
ByteAddressBuffer s_indexBuffer : register(t2);
ConstantBuffer<ClosestHitMatrices> s_matrixBuffer : register(b0);
ConstantBuffer<Material> s_material : register(b1);
ConstantBuffer<ClosestHitConstants> s_closestHitConstants : register(b2);

// ConstantBuffer<RayGenConstantBuffer> s_rayGenConstants : register(b0);

struct RayPayload {
  float3 Color;
  float3 Throughput;
  uint Bounces;
  uint RngState;
};

uint JenkinsHash(uint x) {
  x += x << 10;
  x ^= x >> 6;
  x += x << 3;
  x ^= x >> 11;
  x += x << 15;

  return x;
}

uint InitRngSeed(uint2 pixel, uint sampleVal) {
  uint rngState = dot(pixel, uint2(1, 10000)) ^ JenkinsHash(sampleVal);
  return JenkinsHash(rngState);
}

[shader("raygeneration")]
void RayGenShader() {
  float2 lerpValues = (float2)DispatchRaysIndex() / (float2)DispatchRaysDimensions();

  // float viewportX = lerp(s_rayGenConstants.Viewport.Left, s_rayGenConstants.Viewport.Right,
  //                         lerpValues.x);
  // float viewportY = lerp(s_rayGenConstants.Viewport.Top, s_rayGenConstants.Viewport.Bottom,
  //                         lerpValues.y);

  float viewportX = lerp(-1.33f, 1.33f, lerpValues.x);
  float viewportY = lerp(1.f, -1.f, lerpValues.y);

  uint2 pixel = DispatchRaysIndex().xy;

  float3 accum = 0.f;

  int numSamples = 100;

  for (int i = 0; i < numSamples; ++i) {
    RayDesc ray;
    ray.Origin = float3(0.f, 1.f, -4.f);
    ray.Direction = float3(viewportX * 0.414f, viewportY * 0.414f, 1.f);
    ray.TMin = 0.0;
    ray.TMax = 10000.0;
    
    RayPayload payload;
    payload.Color = float3(0.f, 0.f, 0.f);
    payload.Throughput = float3(1.f, 1.f, 1.f);
    payload.Bounces = 0;
    payload.RngState = InitRngSeed(pixel, i);

    TraceRay(s_scene, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, ~0, 0, 1, 0, ray, payload);

    accum += payload.Color;
  }

  s_film[DispatchRaysIndex().xy] = float4(accum / (float)numSamples, 1.f);
}

typedef BuiltInTriangleIntersectionAttributes IntersectAttributes;

uint3 LoadTriangleIndices(uint offsetBytes) {
  uint alignedOffset = offsetBytes & ~3; // Align to 4 byte boundary
  uint2 two32BitVals = s_indexBuffer.Load2(alignedOffset);

  uint3 indices;

  if (alignedOffset == offsetBytes) {
    indices.x = two32BitVals.x & 0xffff;
    indices.y = two32BitVals.x >> 16;
    indices.z = two32BitVals.y & 0xffff;
  } else {
    indices.x = two32BitVals.x >> 16;
    indices.y = two32BitVals.y & 0xffff;
    indices.z = two32BitVals.y >> 16;
  }

  return indices;
}

float3 LoadNormal(uint indexBufferIndex) {
  uint3 three32BitVals =
      s_normalBuffer.Load3(indexBufferIndex * s_closestHitConstants.NormalBufferStride);
  return asfloat(three32BitVals);
}

float3 InterpolateVertexAttr(float3 vertAttr[3], IntersectAttributes attr) {
  return vertAttr[0] +
         attr.barycentrics.x * (vertAttr[1] - vertAttr[0]) +
         attr.barycentrics.y * (vertAttr[2] - vertAttr[0]);
}

struct ShadowRayPayload {
  bool IsOccluded;
};

static const float PI = 3.14159265f;

float GGX_D(float3 n, float3 h, float alpha) {
  float alphaSq = alpha * alpha;
  float nDotH = dot(n, h);

  float f = (nDotH * nDotH) * (alphaSq - 1.f) + 1.f;

  return alphaSq / (PI * f * f);
}

float GGX_V(float3 Lo, float3 Li, float3 n, float alpha) {
  float alphaSq = alpha * alpha;
  float nDotLo = dot(n, Lo);
  float nDotLi = dot(n, Li);

  float denom1 = nDotLo * sqrt(nDotLi * nDotLi * (1.f - alphaSq) + alphaSq);
  float denom2 = nDotLi * sqrt(nDotLo * nDotLo * (1.f - alphaSq) + alphaSq);

  float denom = denom1 + denom2;
  if (denom > 0.0)
    return 0.5f / denom;

  return 0.f;
}

float3 Brdf(float3 Vo, float3 Vi, float3 n, float roughness, float metallic, float3 baseColor) {
  float3 h = normalize(Vo + Vi);
  
  float3 black = 0.f;
  float3 f0 = lerp(0.04f, baseColor, metallic);
  float alpha = pow(roughness, 2);

  float3 fresnel = f0 + (1.f - f0) * pow((1.f - abs(dot(Vo, h))), 5);
  
  float3 cDiff = lerp(baseColor, black, metallic);

  float3 diffuse = (1.f - fresnel) * (1.f / PI) * cDiff;
  float3 specular = fresnel * GGX_D(n, h, alpha) * GGX_V(Vo, Vi, n, alpha);

  return diffuse + specular;
}

// RNG taken from Ch14 of Ray Tracing Gems II.

uint XorShift(inout uint rngState) {
  rngState ^= (rngState << 13);
  rngState ^= (rngState >> 17);
  rngState ^= (rngState << 5);

  return rngState; 
}

float uintToFloat(uint x) {
  return asfloat(0x3f800000 | (x >> 9)) - 1.f;
}

float Rand(inout uint rngState) {
  return uintToFloat(XorShift(rngState));
}

// From Ch13 of pbrt book.
float2 ConcentricSampleDisk(float2 randPt) {
  float2 offset = 2.f * randPt - float2(1.f, 1.f);

  if (offset.x == 0 && offset.y == 0) return float2(0.f, 0.f);

  float theta = 0.f;
  float r = 0.f;

  if (abs(offset.x) > abs(offset.y)) {
    r = offset.x;
    theta = PI / 4.f * (offset.y / offset.x);
  } else {
    r = offset.y;
    theta = PI / 2.f - PI / 4.f * (offset.x / offset.y);
  }

  return r * float2(cos(theta), sin(theta));
}

float3 CosineSampleHemisphere(float2 randPt) {
  float2 d = ConcentricSampleDisk(randPt);

  float y = sqrt(max(0.f, 1.f - d.x * d.x - d.y * d.y));

  return float3(d.r, y, d.g);
}

void GetCoordinateSystem(float3 v1, inout float3 v2, inout float3 v3) {
  if (abs(v1.x) > abs(v1.y)) {
    v2 = float3(-v1.z, 0.f, v1.x) / sqrt(v1.x * v1.x + v1.z * v1.z);
  } else {
    v2 = float3(0.f, v1.z, -v1.y) / sqrt(v1.y * v1.y + v1.z * v1.z);
  }
  v3 = cross(v1, v2);
}

[shader("closesthit")]
void ClosestHitShader(inout RayPayload payload, IntersectAttributes attr) {
  // Stride of indices in triangle is index size in bytes * indices per triangle => 2 * 3 = 6.
  uint indexBufferOffset = PrimitiveIndex() * 2 * 3;

  uint3 indices = LoadTriangleIndices(indexBufferOffset);
  float3 normals[3] = {
    LoadNormal(indices.x), LoadNormal(indices.y), LoadNormal(indices.z)
  };

  float3 normal = normalize(InterpolateVertexAttr(normals, attr));
  normal = normalize(mul(float3x3(s_matrixBuffer.Transform[0].xyz,
                                  s_matrixBuffer.Transform[1].xyz,
                                  s_matrixBuffer.Transform[2].xyz), normal));

  float3 hitPos = WorldRayOrigin() + RayTCurrent() * WorldRayDirection();

  uint rngState = payload.RngState;

  float3 lightEmissive = 50.f;

  if (payload.Bounces <= 3) {
    float3 dirSample = CosineSampleHemisphere(float2(Rand(rngState), Rand(rngState)));

    float3 nx = 0.f;
    float3 nz = 0.f;

    GetCoordinateSystem(normal, nx, nz);

    float3 rayDir = normalize(dirSample.x * nx + dirSample.y * normal + dirSample.z * nz);
    float pdf = dot(rayDir, normal) / PI;

    RayDesc ray;
    ray.Origin = hitPos;
    ray.Direction = rayDir;
    ray.TMin = 0.0;
    ray.TMax = 10000.0;

    float3 brdf = Brdf(-normalize(WorldRayDirection()), rayDir, normal, 
                       s_material.Roughness.RoughnessFactor, s_material.Roughness.MetallicFactor,
                       s_material.Roughness.BaseColorFactor.rgb);        

    RayPayload reflectPayload;
    reflectPayload.Color = float3(0.f, 0.f, 0.f);
    reflectPayload.Throughput = payload.Throughput * brdf * dot(rayDir, normal) / pdf;
    reflectPayload.Bounces = payload.Bounces + 1;
    reflectPayload.RngState = payload.RngState;

    TraceRay(s_scene, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, ~0, 0, 1, 0, ray, reflectPayload);

    payload.Color += reflectPayload.Color;
  }

  float lightPtX1 = -0.24f;
  float lightPtX2 = 0.23f;

  float lightPtZ1 = -0.22f;
  float lightPtZ2 = 0.16f;

  float3 lightPos = {lerp(lightPtX1, lightPtX2, Rand(rngState)), 1.98f, 
                      lerp(lightPtZ1, lightPtZ2, Rand(rngState))};

  float3 lightDistVec = lightPos - hitPos;
  float3 lightDir = normalize(lightDistVec);
  
  RayDesc shadowRay;
  shadowRay.Origin = hitPos;
  shadowRay.Direction = lightDir;
  shadowRay.TMin = 0.0;
  shadowRay.TMax = length(lightDistVec);
  ShadowRayPayload shadowPayload = { true };

  TraceRay(s_scene,
          RAY_FLAG_CULL_BACK_FACING_TRIANGLES | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH |
          RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER, ~0, 0, 1, 1, shadowRay,
          shadowPayload);

  float3 brdf = Brdf(-normalize(WorldRayDirection()), lightDir, normal, 
                     s_material.Roughness.RoughnessFactor, s_material.Roughness.MetallicFactor,
                     s_material.Roughness.BaseColorFactor.rgb);        

  float isIlluminated = shadowPayload.IsOccluded ? 0.0 : 1.0;

  float3 lightNormal = {0.f, -1.f, 0.f};

  float3 wi = normalize(hitPos - lightPos);
  float distSq = pow(distance(hitPos, lightPos), 2);

  float lightArea = (lightPtX2 - lightPtX1) * (lightPtZ2 - lightPtZ1);
  float pdf = distSq / (abs(dot(lightNormal, -wi)) * lightArea);

  payload.Color += isIlluminated * payload.Throughput * brdf * dot(lightDir, normal) * lightEmissive / pdf;
}

[shader("miss")]
void LightMissShader(inout RayPayload payload) {
  payload.Color = float3(0.f, 0.f, 0.f);
}

[shader("miss")]
void ShadowMissShader(inout ShadowRayPayload payload) {
  payload.IsOccluded = false;
}
