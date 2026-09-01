// fullscreen.slang
// Fullscreen triangle utility for post-processing and deferred passes

struct FullscreenVertexOutput {
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

// Generates fullscreen triangle vertices from vertex ID (0, 1, 2)
// Fits the entire screen:
//   vertex 0: (-1, -1) uv (0, 0)
//   vertex 1: ( 3, -1) uv (2, 0)
//   vertex 2: (-1,  3) uv (0, 2)
FullscreenVertexOutput getFullscreenTriangleVertex(uint vertexId) {
    FullscreenVertexOutput output;
    output.uv = float2((vertexId << 1) & 2, vertexId & 2);
    output.position = float4(output.uv * 2.0 - 1.0, 0.0, 1.0);
    return output;
}

[shader("mesh")]
[outputtopology("triangle")]
[numthreads(3, 1, 1)]
void fullscreenMesh(
    in uint tig : SV_GroupIndex,
    OutputVertices<FullscreenVertexOutput, 3> verts,
    OutputIndices<uint3, 1> triangles
) {
    SetMeshOutputCounts(3, 1);

    if (tig < 3) {
        verts[tig] = getFullscreenTriangleVertex(tig);
    }

    if (tig == 0) {
        triangles[0] = uint3(0, 1, 2);
    }
}
