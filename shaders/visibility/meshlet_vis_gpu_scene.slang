// meshlet_vis_gpu_scene.slang
// Visibility Buffer rasterization pass with indirect GPU scene multi-instance support

struct VisibilityPushConstants {
    float4x4 viewProj;
    uint isIndirect;
    uint meshletOffset;
    uint instanceId;
    uint padding;
};

[[vk::push_constant]]
VisibilityPushConstants pushConstants;

struct GpuInstanceData {
    float4x4 modelMatrix;
    float4x4 normalMatrix;
    float4 boundingSphere; // xyz = center, w = radius
    float4 aabbMin;        // xyz = min, w = meshletOffset
    float4 aabbMax;        // xyz = max, w = meshletCount
    uint meshId;
    uint instanceId;
    uint materialId;
    uint flags;
};

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

struct MeshletDrawTask {
    uint instanceId;
    uint meshletId;
};

[[vk::binding(0, 0)]] StructuredBuffer<GpuInstanceData> instances;
[[vk::binding(1, 0)]] StructuredBuffer<Meshlet> meshlets;
[[vk::binding(2, 0)]] StructuredBuffer<Vertex> vertices;
[[vk::binding(3, 0)]] StructuredBuffer<uint> meshletVertices;
[[vk::binding(4, 0)]] StructuredBuffer<uint> meshletTriangles;
[[vk::binding(5, 0)]] StructuredBuffer<MeshletDrawTask> culledMeshlets;

struct TaskPayload {
    uint instanceId;
    uint meshletIndex;
};

[shader("amplification")]
[numthreads(1, 1, 1)]
void taskMain(in uint3 groupID : SV_GroupID) {
    TaskPayload payload;
    if (pushConstants.isIndirect != 0) {
        MeshletDrawTask task = culledMeshlets[groupID.x];
        payload.instanceId = task.instanceId;
        payload.meshletIndex = task.meshletId;
    } else {
        payload.instanceId = pushConstants.instanceId;
        payload.meshletIndex = pushConstants.meshletOffset + groupID.x;
    }
    DispatchMesh(1, 1, 1, payload);
}

struct VertexOutput {
    float4 position : SV_Position;
    nointerpolation uint meshletId : MESHLET_ID;
    nointerpolation uint instanceId : INSTANCE_ID;
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
    uint instanceIdx = payload.instanceId;
    uint meshletIdx = payload.meshletIndex;

    GpuInstanceData inst = instances[instanceIdx];
    Meshlet meshlet = meshlets[meshletIdx];

    SetMeshOutputCounts(meshlet.vertexCount, meshlet.primitiveCount);

    if (tig < meshlet.vertexCount) {
        uint globalVertexIdx = meshletVertices[meshlet.vertexOffset + tig];
        Vertex v = vertices[globalVertexIdx];
        float4 worldPos = mul(inst.modelMatrix, float4(v.position, 1.0));
        float4 clipPos = mul(pushConstants.viewProj, worldPos);

        VertexOutput vo;
        vo.position = clipPos;
        vo.meshletId = meshlet.meshletId;
        vo.instanceId = instanceIdx;
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
        po.triangleId = meshlet.primitiveOffset + p + 1;
        outPrimitives[p] = po;
    }
}

[shader("fragment")]
uint2 fragMain(VertexOutput vo, PrimitiveOutput po) : SV_Target {
    // Output: RG32_UINT:
    // x = triangleId (global primitive ID, 1-based)
    // y = ((instanceId + 1) << 16) | (meshletId & 0xFFFF)
    return uint2(po.triangleId, ((vo.instanceId + 1) << 16) | (vo.meshletId & 0xFFFF));
}
