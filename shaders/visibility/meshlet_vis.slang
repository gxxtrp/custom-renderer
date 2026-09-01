// meshlet_vis.slang
// Visibility Buffer rasterization pass using task and mesh shaders

struct VisibilityPushConstants {
    float4x4 viewProj;
    float4x4 model;
    uint instanceId;
    uint meshletOffset;
};

[[vk::push_constant]]
VisibilityPushConstants pushConstants;

struct Meshlet {
    float3 center;
    float radius;
    float3 coneAxis;
    float coneCutoff;
    float3 coneApex;
    uint meshletId;
    uint vertexOffset;
    uint vertexCount;
    uint primitiveOffset;
    uint primitiveCount;
};

struct Vertex {
    float3 position;
    float3 normal;
    float2 uv;
};

[[vk::binding(0, 0)]] StructuredBuffer<Meshlet> meshlets;
[[vk::binding(1, 0)]] StructuredBuffer<Vertex> vertices;
[[vk::binding(2, 0)]] StructuredBuffer<uint> meshletVertices;
[[vk::binding(3, 0)]] StructuredBuffer<uint> meshletTriangles;

struct TaskPayload {
    uint meshletIndex;
};

[shader("amplification")]
[numthreads(1, 1, 1)]
void taskMain(in uint3 groupID : SV_GroupID) {
    TaskPayload payload;
    payload.meshletIndex = pushConstants.meshletOffset + groupID.x;
    DispatchMesh(1, 1, 1, payload);
}

struct VertexOutput {
    float4 position : SV_Position;
    nointerpolation uint meshletId : MESHLET_ID;
};

struct PrimitiveOutput {
    nointerpolation uint triangleId : SV_PrimitiveID;
};

[shader("mesh")]
[outputtopology("triangle")]
[numthreads(64, 1, 1)]
void meshMain(
    in uint tig : SV_GroupIndex,
    in payload TaskPayload payload,
    OutputVertices<VertexOutput, 64> outVertices,
    OutputIndices<uint3, 126> outTriangles,
    OutputPrimitives<PrimitiveOutput, 126> outPrimitives
) {
    uint meshletIdx = payload.meshletIndex;
    Meshlet meshlet = meshlets[meshletIdx];

    SetMeshOutputCounts(meshlet.vertexCount, meshlet.primitiveCount);

    if (tig < meshlet.vertexCount) {
        uint globalVertexIdx = meshletVertices[meshlet.vertexOffset + tig];
        Vertex v = vertices[globalVertexIdx];
        float4 worldPos = mul(pushConstants.model, float4(v.position, 1.0));
        float4 clipPos = mul(pushConstants.viewProj, worldPos);

        VertexOutput vo;
        vo.position = clipPos;
        vo.meshletId = meshlet.meshletId;
        outVertices[tig] = vo;
    }

    for (uint p = tig; p < meshlet.primitiveCount; p += 64) {
        uint packedTri = meshletTriangles[meshlet.primitiveOffset + p];
        uint3 triIndices = uint3(
            packedTri & 0xFF,
            (packedTri >> 8) & 0xFF,
            (packedTri >> 16) & 0xFF
        );
        outTriangles[p] = triIndices;

        PrimitiveOutput po;
        // 1-based global triangle ID so 0 can be background
        po.triangleId = meshlet.primitiveOffset + p + 1;
        outPrimitives[p] = po;
    }
}

[shader("fragment")]
uint2 fragMain(VertexOutput vo, PrimitiveOutput po) : SV_Target {
    // Output: RG32_UINT: x = triangleId, y = meshletId + 1
    return uint2(po.triangleId, vo.meshletId + 1);
}
