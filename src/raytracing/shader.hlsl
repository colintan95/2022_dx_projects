struct Viewport {
  float Left;
  float Top;
  float Right;
  float Bottom;
};

struct RayGenConstantBuffer {
  Viewport Viewport;
};

typedef BuiltInTriangleIntersectionAttributes IntersectAttributes;

struct RayPayload {
  float4 Color;
};

// Global descriptors.

RaytracingAccelerationStructure s_scene : register(t0);
RWTexture2D<float4> s_film : register(u0);

// Ray generation descriptors.

// ConstantBuffer<RayGenConstantBuffer> s_rayGenConstants : register(b0);

[shader("raygeneration")]
void RayGenShader() {
  float2 lerpValues = (float2)DispatchRaysIndex() / (float2)DispatchRaysDimensions();

  // float viewportX = lerp(s_rayGenConstants.Viewport.Left, s_rayGenConstants.Viewport.Right,
  //                         lerpValues.x);
  // float viewportY = lerp(s_rayGenConstants.Viewport.Top, s_rayGenConstants.Viewport.Bottom,
  //                         lerpValues.y);

  float viewportX = lerp(1.33f, -1.33f, lerpValues.x);
  float viewportY = lerp(1.f, -1.f, lerpValues.y);

  RayDesc ray;
  ray.Origin = float3(0.f, 0.f, -2.f);
  ray.Direction = float3(viewportX * 0.414f, viewportY * 0.414f, 1.f);
  ray.TMin = 0.0;
  ray.TMax = 10000.0;
  RayPayload payload = { float4(0.f, 0.f, 0.f, 0.f) };

  TraceRay(s_scene, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, ~0, 0, 1, 0, ray, payload);

  s_film[DispatchRaysIndex().xy] = payload.Color;
}

[shader("closesthit")]
void ClosestHitShader(inout RayPayload payload, IntersectAttributes intersectAttr) {
  payload.Color = float4(1.f, 0.f, 0.f, 1.f);
}

[shader("miss")]
void MissShader(inout RayPayload payload) {
  payload.Color = float4(0.f, 0.f, 0.f, 1.f);
}
