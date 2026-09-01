// meshlet_cull.slang
// Compute shader for meshlet frustum culling and normal cone backface culling

#include "cull_common.slang"

struct MeshletCullPushConstants {
    float4 frustumPlanes[6];
    float4 cameraPos;
    uint instanceCount;
    uint meshletsPerInstance;
    uint enableConeCull;
    uint padding;
};

[[vk::push_constant]]
MeshletCullPushConstants pushConstants;

[[vk::binding(0, 0)]] StructuredBuffer<GpuInstanceData> instances;
[[vk::binding(1, 0)]] StructuredBuffer<Meshlet> meshlets;
[[vk::binding(2, 0)]] StructuredBuffer<uint> visibleInstanceIndices;
[[vk::binding(3, 0)]] StructuredBuffer<uint> visibleInstanceCount;
[[vk::binding(4, 0)]] RWStructuredBuffer<DrawMeshTasksIndirectCommand> indirectDrawCommand;
[[vk::binding(5, 0)]] RWStructuredBuffer<MeshletDrawTask> culledMeshletList;
[[vk::binding(6, 0)]] RWStructuredBuffer<uint> culledMeshletCount;

[shader("compute")]
[numthreads(64, 1, 1)]
void computeMain(uint3 dispatchThreadId : SV_DispatchThreadID) {
    uint totalThreads = pushConstants.instanceCount * pushConstants.meshletsPerInstance;
    uint globalThreadId = dispatchThreadId.x;

    if (globalThreadId >= totalThreads) {
        return;
    }

    uint instanceId = globalThreadId / pushConstants.meshletsPerInstance;
    uint localMeshletIdx = globalThreadId % pushConstants.meshletsPerInstance;

    GpuInstanceData inst = instances[instanceId];
    uint meshletCount = uint(inst.aabbMax.w);
    if (localMeshletIdx >= meshletCount) {
        return;
    }

    // Fast check: is instance inside frustum?
    if (!isAabbInsideFrustum(inst.aabbMin.xyz, inst.aabbMax.xyz, pushConstants.frustumPlanes)) {
        return;
    }

    uint meshletOffset = uint(inst.aabbMin.w);
    uint globalMeshletIdx = meshletOffset + localMeshletIdx;
    Meshlet m = meshlets[globalMeshletIdx];

    // 1. Meshlet Bounding Sphere Frustum Test
    float3 worldCenter = mul(inst.modelMatrix, float4(m.center, 1.0)).xyz;
    float maxScale = max(length(inst.modelMatrix[0].xyz),
                         max(length(inst.modelMatrix[1].xyz), length(inst.modelMatrix[2].xyz)));
    float worldRadius = m.radius * maxScale;

    if (!isSphereInsideFrustum(worldCenter, worldRadius, pushConstants.frustumPlanes)) {
        return;
    }

    // 2. Normal Cone Backface Culling Test
    if (pushConstants.enableConeCull != 0 && m.coneCutoff > 0.0 && m.coneCutoff < 1.0) {
        float3 axis = mul((float3x3)inst.normalMatrix, m.coneAxis);
        float axisLen = length(axis);
        if (axisLen > 1e-4) {
            float3 worldConeAxis = axis / axisLen;
            if (isConeBackfacing(worldCenter, worldRadius, worldConeAxis, m.coneCutoff, pushConstants.cameraPos.xyz)) {
                return;
            }
        }
    }

    // 3. Surviving meshlet: atomically append to indirect draw argument buffer and payload list
    uint slot = 0;
    InterlockedAdd(indirectDrawCommand[0].groupCountX, 1, slot);

    MeshletDrawTask task;
    task.instanceId = instanceId;
    task.meshletId = globalMeshletIdx;
    culledMeshletList[slot] = task;

    InterlockedAdd(culledMeshletCount[0], 1);
}
