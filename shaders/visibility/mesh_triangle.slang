// mesh_triangle.slang
// Basic mesh shader pipeline rendering a single colored triangle with reverse-Z depth testing

struct TaskPayload {
    uint dispatchId;
};

[shader("amplification")]
[numthreads(1, 1, 1)]
void taskMain() {
    TaskPayload payload;
    payload.dispatchId = 0;
    DispatchMesh(1, 1, 1, payload);
}

struct VertexOutput {
    float4 position : SV_Position;
    float4 color : COLOR0;
};

[shader("mesh")]
[outputtopology("triangle")]
[numthreads(3, 1, 1)]
void meshMain(
    in uint tig : SV_GroupIndex,
    in payload TaskPayload payload,
    OutputVertices<VertexOutput, 3> verts,
    OutputIndices<uint3, 1> triangles
) {
    SetMeshOutputCounts(3, 1);

    // Positions in clip space:
    // Vulkan Y is downwards in normalized device coordinates
    static const float2 positions[3] = {
        float2( 0.0, -0.5),  // Top
        float2( 0.5,  0.5),  // Bottom right
        float2(-0.5,  0.5)   // Bottom left
    };

    static const float3 colors[3] = {
        float3(1.0, 0.0, 0.0),  // Red
        float3(0.0, 1.0, 0.0),  // Green
        float3(0.0, 0.0, 1.0)   // Blue
    };

    if (tig < 3) {
        VertexOutput v;
        // In reverse-Z, near plane is 1.0 and far plane is 0.0.
        // With depth buffer cleared to 0.0 and CompareOp::GreaterOrEqual,
        // depth value 0.5f passes the depth test.
        v.position = float4(positions[tig], 0.5, 1.0);
        v.color = float4(colors[tig], 1.0);
        verts[tig] = v;
    }

    if (tig == 0) {
        // Triangle primitive indices
        triangles[0] = uint3(0, 1, 2);
    }
}

[shader("fragment")]
float4 fragMain(VertexOutput input) : SV_Target {
    return input.color;
}
