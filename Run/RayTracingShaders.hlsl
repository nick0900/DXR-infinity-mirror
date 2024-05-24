RaytracingAccelerationStructure gRtScene : register(t0);
RWTexture2D<float4> gOutput : register(u0);

cbuffer CB_Global : register(b0, space0)
{
    uint MaxRecursion;
    float ReflectionBias;
}

//Mirror Resources
struct Vertex
{
    float3 pos;
    float3 norm;
    float2 uv;
};

StructuredBuffer<Vertex> Vertecies : register(t1);
StructuredBuffer<uint> Indecies : register(t2);

//Edges Resources
cbuffer CB_EdgesShaderTableLocal : register(b0, space2)
{
	float3 ShaderTableColor;
}

struct RayPayload
{
	float3 color;
    uint depth;
};

[shader("raygeneration")]
void rayGen()
{
	uint3 launchIndex = DispatchRaysIndex();
	uint3 launchDim = DispatchRaysDimensions();

	float2 crd = float2(launchIndex.xy);
	float2 dims = float2(launchDim.xy);

	float2 d = ((crd / dims) * 2.f - 1.f);
	float aspectRatio = dims.x / dims.y;

	RayDesc ray;
	ray.Origin = float3(0, 0, -1.5f);
	ray.Direction = normalize(float3(d.x * aspectRatio, -d.y, 1));

	ray.TMin = 0;
	ray.TMax = 100000;

    RayPayload payload = { float3(1.0f, 1.0f, 1.0f), 1 };
    TraceRay(gRtScene, RAY_FLAG_CULL_BACK_FACING_TRIANGLES /*rayFlags*/, 0xFF, 0 /* ray index*/, 0, 0, ray, payload);
	gOutput[launchIndex.xy] = float4(payload.color, 1);
}

[shader("miss")]
void miss(inout RayPayload payload)
{
	payload.color = float3(0.0f, 0.0f, 0.0f);
}

[shader("closesthit")]
void closestHit_mirror(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attribs)
{
	//for info
    float3 barycentrics = float3(1.0 - attribs.barycentrics.x - attribs.barycentrics.y, attribs.barycentrics.x, attribs.barycentrics.y);
    uint instanceID = InstanceID();
    uint primitiveID = PrimitiveIndex();
    
    float absorption = 1.0f / float(MaxRecursion);
    payload.color -= float3(absorption, absorption, absorption);
    
    if (payload.depth >= MaxRecursion)
        return;
    
    payload.depth++;
	
    Vertex vtx0 = Vertecies[Indecies[primitiveID * 3 + 0]];
    Vertex vtx1 = Vertecies[Indecies[primitiveID * 3 + 1]];
    Vertex vtx2 = Vertecies[Indecies[primitiveID * 3 + 2]];
	
    float3 interPos = vtx0.pos * barycentrics.x + vtx1.pos * barycentrics.y + vtx2.pos * barycentrics.z;
    float3 interNorm = normalize(vtx0.norm * barycentrics.x + vtx1.norm * barycentrics.y + vtx2.norm * barycentrics.z);
    
    float3 worldRayOrigin = mul(float4(interPos, 1.0f), ObjectToWorld4x3());
    float3 worldNormal = normalize(mul(interNorm, (float3x3) ObjectToWorld4x3()));
    worldRayOrigin += worldNormal * ReflectionBias;
    
    RayDesc ray;
    ray.Origin = worldRayOrigin;
    ray.Direction = normalize(reflect(WorldRayDirection(), worldNormal));
    
    ray.TMin = 0;
    ray.TMax = 100000;
    //payload.color = worldNormal;
    TraceRay(gRtScene, RAY_FLAG_CULL_BACK_FACING_TRIANGLES /*rayFlags*/, 0xFF, 0 /* ray index*/, 0, 0, ray, payload);
}

[shader("closesthit")]
void closestHit_edges(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attribs)
{
	//for info
    //float3 barycentrics = float3(1.0 - attribs.barycentrics.x - attribs.barycentrics.y, attribs.barycentrics.x, attribs.barycentrics.y);
    //uint instanceID = InstanceID();
    //uint primitiveID = PrimitiveIndex();

    payload.color *= ShaderTableColor;
}
