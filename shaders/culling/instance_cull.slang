// instance_cull.slang
// Compute shader for instance-level frustum culling

#include "cull_common.slang"

struct InstanceCullPushConstants {
    float4 frustumPlanes[6];
    float4 cameraPos;
    uint instanceCount;
    uint padding;
};

[[vk::push_constant]]
InstanceCullPushConstants pushConstants;

[[vk::binding(0, 0)]] StructuredBuffer<GpuInstanceData> instances;
[[vk::binding(1, 0)]] RWStructuredBuffer<uint> visibleInstanceIndices;
[[vk::binding(2, 0)]] RWStructuredBuffer<uint> visibleInstanceCount;
[[vk::binding(3, 0)]] RWStructuredBuffer<DrawMeshTasksIndirectCommand> indirectDrawCommand;

[shader("compute")]
[numthreads(64, 1, 1)]
void computeMain(uint3 dispatchThreadId : SV_DispatchThreadID) {
    uint instanceId = dispatchThreadId.x;
    if (instanceId >= pushConstants.instanceCount) {
        return;
    }

    GpuInstanceData inst = instances[instanceId];

    // Hierarchical test: AABB first, then bounding sphere
    bool isVisible = isAabbInsideFrustum(inst.aabbMin.xyz, inst.aabbMax.xyz, pushConstants.frustumPlanes);
    if (isVisible) {
        isVisible = isSphereInsideFrustum(inst.boundingSphere.xyz, inst.boundingSphere.w, pushConstants.frustumPlanes);
    }

    if (isVisible) {
        uint slot = 0;
        InterlockedAdd(visibleInstanceCount[0], 1, slot);
        visibleInstanceIndices[slot] = instanceId;
    }
}
