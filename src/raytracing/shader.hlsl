#include "shader.h"

struct SampleConstants {
  uint CurrentSample;
  uint SampleIncrement;
  uint NumBounces;
};

// Global descriptors.

RaytracingAccelerationStructure s_scene : register(t0);
RWTexture2D<float4> s_film : register(u0);
ConstantBuffer<SampleConstants> s_sampleConstants : register(b0);

// Closest hit descriptors.

struct ClosestHitMatrices {
  float3x4 Transform;
};

ByteAddressBuffer s_normalBuffer : register(t0, space1);
ByteAddressBuffer s_indexBuffer : register(t1, space1);
ConstantBuffer<ClosestHitMatrices> s_matrixBuffer : register(b0, space1);
ConstantBuffer<Material> s_material : register(b1, space1);
ConstantBuffer<ClosestHitConstants> s_closestHitConstants : register(b2, space1);

// ConstantBuffer<RayGenConstantBuffer> s_rayGenConstants : register(b0);

struct RayPayload {
  float3 L;
  float3 Throughput;
  uint Bounces;
  uint RngState;
};

// RNG taken from Ch14 of Ray Tracing Gems II.

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

uint XorShift(inout uint rngState) {
  rngState ^= (rngState << 13);
  rngState ^= (rngState >> 17);
  rngState ^= (rngState << 5);

  return rngState;
}

float RngStateToFloat(uint rngState) {
  return asfloat(0x3f800000 | (rngState >> 9)) - 1.f;
}

float Rand(inout uint rngState) {
  return RngStateToFloat(XorShift(rngState));
}

[shader("raygeneration")]
void RayGenShader() {
  // float viewportX = lerp(s_rayGenConstants.Viewport.Left, s_rayGenConstants.Viewport.Right,
  //                         lerpValues.x);
  // float viewportY = lerp(s_rayGenConstants.Viewport.Top, s_rayGenConstants.Viewport.Bottom,
  //                         lerpValues.y);

  float n = s_sampleConstants.CurrentSample;
  float k = s_sampleConstants.SampleIncrement;

  uint2 screenPixel = DispatchRaysIndex().xy;
  uint rngState = InitRngSeed(screenPixel, n);

  float3 accumL = 0.f;

  uint screenSamples = 4;

  for (int i = 0; i < screenSamples; ++i) {
    float2 sampledPixel = screenPixel + float2(Rand(rngState), Rand(rngState));

    float2 lerpValues = sampledPixel / (float2)DispatchRaysDimensions();

    float viewportX = lerp(-1.33f, 1.33f, lerpValues.x);
    float viewportY = lerp(1.f, -1.f, lerpValues.y);

    RayDesc ray;
    ray.Origin = float3(0.f, 1.f, -4.f);
    ray.Direction = float3(viewportX * 0.414f, viewportY * 0.414f, 1.f);
    ray.TMin = 0.f;
    ray.TMax = 10000.f;

    for (int j = 0; j < k; ++j) {
      RayPayload payload;
      payload.L = float3(0.f, 0.f, 0.f);
      payload.Throughput = float3(1.f, 1.f, 1.f);
      payload.Bounces = 0;
      payload.RngState = InitRngSeed(screenPixel, n + j);

      TraceRay(s_scene, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, ~0, 0, 1, 0, ray, payload);

      accumL += payload.L;
    }
  }

  accumL /= (float)screenSamples;

  float3 prevFilmVal = s_film[screenPixel].rgb;

  s_film[screenPixel].rgb = (1.f / (n + k)) * (n * prevFilmVal + accumL);
  s_film[screenPixel].a = 1.f;
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

float TrowbridgeReitzGGX_Microfacet(float3 n, float3 h, float alpha) {
  float alphaSq = alpha * alpha;
  float nDotH = dot(n, h);

  float f = (nDotH * nDotH) * (alphaSq - 1.f) + 1.f;

  return alphaSq / (PI * f * f);
}

float TrowbridgeReitzGGX_Visibility(float3 wo, float3 wi, float3 n, float alpha) {
  float alphaSq = alpha * alpha;
  float nDotwo = dot(n, wo);
  float nDotwi = dot(n, wi);

  float denom1 = nDotwo * sqrt(nDotwi * nDotwi * (1.f - alphaSq) + alphaSq);
  float denom2 = nDotwi * sqrt(nDotwo * nDotwo * (1.f - alphaSq) + alphaSq);

  float denom = denom1 + denom2;
  if (denom > 0.f)
    return 0.5f / denom;

  return 0.f;
}

float3 Brdf(float3 wo, float3 wi, float3 n, float roughness, float metallic, float3 baseColor) {
  float alpha = pow(roughness, 2);
  float3 h = normalize(wo + wi);

  float D = TrowbridgeReitzGGX_Microfacet(n, h, alpha);
  float V = TrowbridgeReitzGGX_Visibility(wo, wi, n, alpha);

  float3 black = 0.f;
  float3 cDiff = lerp(baseColor, black, metallic);

  float3 f0 = lerp(0.04f, baseColor, metallic);
  float3 fresnel = f0 + (1.f - f0) * pow((1.f - abs(dot(wo, h))), 5);

  float3 diffuse = (1.f - fresnel) * (1.f / PI) * cDiff;
  float3 specular = fresnel * D * V;

  return diffuse + specular;
}

// Sampling from Ch13 of pbrt book.

float2 ConcentricSampleDisk(float2 randPt) {
  float2 offset = 2.f * randPt - 1.f;

  if (offset.x == 0.f && offset.y == 0.f) return 0.f;

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

  float3 wo = -normalize(WorldRayDirection());

  if (payload.Bounces < s_sampleConstants.NumBounces) {
    float3 wi = CosineSampleHemisphere(float2(Rand(rngState), Rand(rngState)));

    float3 b1 = 0.f;
    float3 b2 = 0.f;
    GetCoordinateSystem(normal, b1, b2);

    wi = normalize(wi.x * b1 + wi.y * normal + wi.z * b2);
    float pdf = dot(wi, normal) / PI;

    RayDesc ray;
    ray.Origin = hitPos;
    ray.Direction = wi;
    ray.TMin = 0.f;
    ray.TMax = 10000.f;

    float3 brdf = Brdf(wo, wi, normal, s_material.Roughness, s_material.Metallic,
                       s_material.BaseColor.rgb);

    RayPayload reflectPayload;
    reflectPayload.L = float3(0.f, 0.f, 0.f);
    reflectPayload.Throughput = payload.Throughput * brdf * dot(wi, normal) / pdf;
    reflectPayload.Bounces = payload.Bounces + 1;
    reflectPayload.RngState = payload.RngState ^ JenkinsHash(reflectPayload.Bounces);

    TraceRay(s_scene, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, ~0, 0, 1, 0, ray, reflectPayload);

    payload.L += reflectPayload.L;
  }

  float lightPtX1 = -0.25f;
  float lightPtX2 = 0.25f;

  float lightPtZ1 = -0.25f;
  float lightPtZ2 = 0.25f;

  float3 lightSamplePos = float3(lerp(lightPtX1, lightPtX2, Rand(rngState)), 1.98f,
                                 lerp(lightPtZ1, lightPtZ2, Rand(rngState)));

  float3 lightNormal = float3(0.f, -1.f, 0.f);
  float lightArea = (lightPtX2 - lightPtX1) * (lightPtZ2 - lightPtZ1);
  float lightDist = distance(hitPos, lightSamplePos);

  float3 Le = 40.f;

  float3 wi = normalize(lightSamplePos - hitPos);
  float pdf = (lightDist * lightDist) / (abs(dot(lightNormal, -wi)) * lightArea);

  RayDesc shadowRay;
  shadowRay.Origin = hitPos;
  shadowRay.Direction = wi;
  shadowRay.TMin = 0.001f;
  shadowRay.TMax = lightDist;

  ShadowRayPayload shadowPayload;
  shadowPayload.IsOccluded = true;

  TraceRay(s_scene,
           RAY_FLAG_CULL_BACK_FACING_TRIANGLES | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH |
           RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER,
           ~0 & (~2), // skips the light geometry - instance mask for the light is 2
           0, 1, 1, shadowRay, shadowPayload);

  if (!shadowPayload.IsOccluded) {
    float3 brdf = Brdf(wo, wi, normal, s_material.Roughness, s_material.Metallic,
                     s_material.BaseColor.rgb);

    payload.L += payload.Throughput * brdf * max(0.f, dot(wi, normal)) * Le / pdf;
  }
}

[shader("miss")]
void LightRayMissShader(inout RayPayload payload) {
  payload.L = float3(0.f, 0.f, 0.f);
}

[shader("miss")]
void ShadowRayMissShader(inout ShadowRayPayload payload) {
  payload.IsOccluded = false;
}

ConstantBuffer<shader::Quad> s_quad : register(b3);

struct QuadIntersectAttributes {
  float Nothing;
};

[shader("intersection")]
void QuadIntersectShader() {
  // shader::Quad quad = s_quad;
  shader::Quad quad;
  quad.BlasToAabb = float4x4(1.f, 0.f, 0.f, 0.f,
                             0.f, 1.f, 0.f, 0.f,
                             0.f, 0.f, 1.f, 0.f,
                             0.f, -1.98f, 0.f, 1.f);
  quad.Width = 0.5f;
  quad.Height = 0.5f;

  float3 rayOrigin = mul(float4(ObjectRayOrigin(), 1.f), quad.BlasToAabb).xyz;
  float3 rayDir = mul(ObjectRayDirection(), (float3x3)quad.BlasToAabb);

  float3 quadOrigin = float3(0.f, 0.f, 0.f);
  float3 quadNormal = float3(0.f, 1.f, 0.f);

  float dDotN = dot(rayDir, quadNormal);
  if (dDotN == 0.f)
    return;

  float tHit = dot(quadOrigin - rayOrigin, quadNormal) / dDotN;
  if (tHit < RayTMin() || tHit > RayTCurrent())
    return;

  float3 intersect = rayOrigin + tHit * rayDir;

  float halfWidth = quad.Width / 2.f;
  float halfHeight = quad.Height / 2.f;

  if (intersect.x > halfWidth || intersect.x < -halfWidth)
    return;

  if (intersect.z > halfHeight || intersect.z < -halfHeight)
    return;

  QuadIntersectAttributes attr = (QuadIntersectAttributes)0;

  ReportHit(tHit, 0, attr);
}

[shader("closesthit")]
void LightClosestHitShader(inout RayPayload payload, QuadIntersectAttributes attr) {
  if (payload.Bounces == 0) {
    payload.L = float3(1.f, 1.f, 1.f);
  } else {
    payload.L = float3(0.f, 0.f, 0.f);
  }
}
