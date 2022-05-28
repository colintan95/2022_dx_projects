#pragma pack_matrix(row_major)

// Global descriptors.

RaytracingAccelerationStructure s_scene : register(t0);
RWTexture2D<float4> s_film : register(u0);

// Closest hit descriptors.

struct ClosestHitMatrices {
  float3x4 Transform;
};

struct ClosestHitConstants {
  uint NormalBufferStride;
};

ByteAddressBuffer s_normalBuffer : register(t1);
ByteAddressBuffer s_indexBuffer : register(t2);
ConstantBuffer<ClosestHitMatrices> s_matrixBuffer : register(b0);
ConstantBuffer<ClosestHitConstants> s_closestHitConstants : register(b1);

// ConstantBuffer<RayGenConstantBuffer> s_rayGenConstants : register(b0);

struct RayPayload {
  float4 Color;
};

[shader("raygeneration")]
void RayGenShader() {
  float2 lerpValues = (float2)DispatchRaysIndex() / (float2)DispatchRaysDimensions();

  // float viewportX = lerp(s_rayGenConstants.Viewport.Left, s_rayGenConstants.Viewport.Right,
  //                         lerpValues.x);
  // float viewportY = lerp(s_rayGenConstants.Viewport.Top, s_rayGenConstants.Viewport.Bottom,
  //                         lerpValues.y);

  float viewportX = lerp(-1.33f, 1.33f, lerpValues.x);
  float viewportY = lerp(1.f, -1.f, lerpValues.y);

  RayDesc ray;
  ray.Origin = float3(0.f, 1.f, -4.f);
  ray.Direction = float3(viewportX * 0.414f, viewportY * 0.414f, 1.f);
  ray.TMin = 0.0;
  ray.TMax = 10000.0;
  RayPayload payload = { float4(0.f, 0.f, 0.f, 0.f) };

  TraceRay(s_scene, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, ~0, 0, 1, 0, ray, payload);

  s_film[DispatchRaysIndex().xy] = payload.Color;
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

[shader("closesthit")]
void ClosestHitShader(inout RayPayload payload, IntersectAttributes attr) {
  // Stride of indices in triangle is index size in bytes * indices per triangle => 2 * 3 = 6.
  uint indexBufferOffset = PrimitiveIndex() * 2 * 3;

  uint3 indices = LoadTriangleIndices(indexBufferOffset);
  float3 triangleNormals[3] = {
    LoadNormal(indices.x), LoadNormal(indices.y), LoadNormal(indices.z)
  };

  float3 normal = normalize(InterpolateVertexAttr(triangleNormals, attr));
  normal = normalize(mul(float3x3(s_matrixBuffer.Transform[0].xyz,
                                  s_matrixBuffer.Transform[1].xyz,
                                  s_matrixBuffer.Transform[2].xyz), normal));

  float3 hitPos = WorldRayOrigin() + RayTCurrent() * WorldRayDirection();

  float3 lightPos = float3(0.f, 1.9f, 0.f);
  float3 lightDir = normalize(lightPos - hitPos);

  float3 ambient = float3(1.f, 1.f, 1.f);
  float3 diffuse = clamp(dot(lightDir, normal), 0.0, 1.0) * float3(1.f, 1.f, 1.f);

  payload.Color = float4(0.1f * ambient + diffuse, 1.f);
}

[shader("miss")]
void MissShader(inout RayPayload payload) {
  payload.Color = float4(0.f, 0.f, 0.f, 1.f);
}
