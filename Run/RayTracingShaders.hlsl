RaytracingAccelerationStructure gRtScene : register(t0);
RWTexture2D<float4> gOutput : register(u0);

cbuffer CB_Global : register(b0, space0)
{
	float RedChannel;
    uint MaxRecursion;
}

//Mirror Resources
struct Vertex
{
    float3 pos;
    float3 norm;
    float2 uv;
};

struct Index
{
    uint i;
};

StructuredBuffer<Vertex> Vertecies : register(t1);
StructuredBuffer<uint> Indecies : register(t2);

cbuffer CB_MirrorShaderTableLocal : register(b0, space1)
{
    float3 TestTableColor;
}

//Edges Resources
cbuffer CB_EdgesShaderTableLocal : register(b0, space2)
{
	float3 ShaderTableColor;
}

struct RayPayload
{
	float3 color;
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
	ray.Origin = float3(0, 0, -2);
	ray.Direction = normalize(float3(d.x * aspectRatio, -d.y, 1));

	ray.TMin = 0;
	ray.TMax = 100000;

    RayPayload payload = { float3(1.0f, 1.0f, 1.0f) };
    TraceRay(gRtScene, RAY_FLAG_CULL_BACK_FACING_TRIANGLES /*rayFlags*/, 0xFF, 0 /* ray index*/, 0, 0, ray, payload);
	gOutput[launchIndex.xy] = float4(payload.color, 1);
}

[shader("miss")]
void miss(inout RayPayload payload)
{
	payload.color = float3(0.4f, 0.6f, 0.3f);
}

[shader("closesthit")]
void closestHit_mirror(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attribs)
{
	//for info
	float3 barycentrics = float3(1.0 - attribs.barycentrics.x - attribs.barycentrics.y, attribs.barycentrics.x, attribs.barycentrics.y);
	uint instanceID = InstanceID();
	uint primitiveID = PrimitiveIndex();
	
    //float3 vtx0 = Vertecies[Indecies[primitiveID * 3 + 0]].pos;
    //Vertex vtx1 = Vertecies[Indecies[primitiveID * 3 + 1]];
    //Vertex vtx2 = Vertecies[Indecies[primitiveID * 3 + 2]];
	
    //float3 interPos = vtx0.pos * barycentrics.x + vtx1.pos * barycentrics.y + vtx2.pos * barycentrics.z;
    //float3 interNorm = normalize(vtx0.norm * barycentrics.x + vtx1.norm * barycentrics.y + vtx2.norm * barycentrics.z);
	
    //float absorption = 1.0f / float(MaxRecursion);
    //payload.color -= float3(absorption, absorption, absorption);
    
    payload.color = TestTableColor;
}

[shader("closesthit")]
void closestHit_edges(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attribs)
{
	//for info
    float3 barycentrics = float3(1.0 - attribs.barycentrics.x - attribs.barycentrics.y, attribs.barycentrics.x, attribs.barycentrics.y);
    uint instanceID = InstanceID();
    uint primitiveID = PrimitiveIndex();

    payload.color *= float3(0, 0, RedChannel) + ShaderTableColor;
}
