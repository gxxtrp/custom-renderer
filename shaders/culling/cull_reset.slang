// cull_reset.slang
// Reset compute pass initializing indirect draw arguments and culling counters

struct DrawMeshTasksIndirectCommand {
    uint groupCountX;
    uint groupCountY;
    uint groupCountZ;
};

[[vk::binding(0, 0)]] RWStructuredBuffer<uint> visibleInstanceCount;
[[vk::binding(1, 0)]] RWStructuredBuffer<DrawMeshTasksIndirectCommand> indirectDrawCommand;
[[vk::binding(2, 0)]] RWStructuredBuffer<uint> culledMeshletCount;

[shader("compute")]
[numthreads(1, 1, 1)]
void computeMain() {
    visibleInstanceCount[0] = 0;
    indirectDrawCommand[0].groupCountX = 0;
    indirectDrawCommand[0].groupCountY = 1;
    indirectDrawCommand[0].groupCountZ = 1;
    culledMeshletCount[0] = 0;
}
