// deferred_texturing.slang
// Deferred texturing resolve pass reconstructing barycentrics and debug visualizations

struct FullscreenVertexOutput {
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

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

struct DeferredPushConstants {
    float4x4 viewProj;
    float4x4 model;
    float4 cameraPos;
    uint2 screenSize;
    uint debugMode;
    uint padding;
};

[[vk::push_constant]]
DeferredPushConstants pushConstants;

struct Vertex {
    float3 position;
    float3 normal;
    float2 uv;
};

[[vk::binding(0, 0)]] Texture2D<uint2> visibilityTexture;
[[vk::binding(1, 0)]] Texture2D<float> depthTexture;
[[vk::binding(2, 0)]] StructuredBuffer<Vertex> vertices;
[[vk::binding(3, 0)]] StructuredBuffer<uint> globalIndices;

float3 hashColor(uint id) {
    uint hash = id * 2654435761u;
    hash = (hash ^ (hash >> 16)) * 2246822519u;
    hash = (hash ^ (hash >> 13)) * 3266489917u;
    hash ^= (hash >> 16);
    float r = float((hash) & 0xFF) / 255.0;
    float g = float((hash >> 8) & 0xFF) / 255.0;
    float b = float((hash >> 16) & 0xFF) / 255.0;
    return float3(r, g, b);
}

[shader("fragment")]
float4 fragMain(FullscreenVertexOutput input) : SV_Target {
    int2 pixelCoord = int2(input.position.xy);
    uint2 vis = visibilityTexture.Load(int3(pixelCoord, 0));
    if (vis.x == 0) {
        float2 ndc = (input.uv - 0.5) * 2.0;
        float vignette = 1.0 - 0.25 * dot(ndc, ndc);
        return float4(float3(0.04, 0.05, 0.08) * vignette, 1.0);
    }

    uint triangleId = vis.x - 1;
    uint meshletId = vis.y > 0 ? vis.y - 1 : 0;

    uint i0 = globalIndices[triangleId * 3 + 0];
    uint i1 = globalIndices[triangleId * 3 + 1];
    uint i2 = globalIndices[triangleId * 3 + 2];

    Vertex v0 = vertices[i0];
    Vertex v1 = vertices[i1];
    Vertex v2 = vertices[i2];

    float4 p0 = mul(pushConstants.viewProj, mul(pushConstants.model, float4(v0.position, 1.0)));
    float4 p1 = mul(pushConstants.viewProj, mul(pushConstants.model, float4(v1.position, 1.0)));
    float4 p2 = mul(pushConstants.viewProj, mul(pushConstants.model, float4(v2.position, 1.0)));

    float2 s0 = ((p0.xy / p0.w) * float2(0.5, 0.5) + 0.5) * float2(pushConstants.screenSize);
    float2 s1 = ((p1.xy / p1.w) * float2(0.5, 0.5) + 0.5) * float2(pushConstants.screenSize);
    float2 s2 = ((p2.xy / p2.w) * float2(0.5, 0.5) + 0.5) * float2(pushConstants.screenSize);

    float2 p = float2(pixelCoord) + 0.5;

    float det = (s1.y - s2.y) * (s0.x - s2.x) + (s2.x - s1.x) * (s0.y - s2.y);
    float invDet = abs(det) > 1e-6 ? (1.0 / det) : 0.0;

    float l0 = ((s1.y - s2.y) * (p.x - s2.x) + (s2.x - s1.x) * (p.y - s2.y)) * invDet;
    float l1 = ((s2.y - s0.y) * (p.x - s2.x) + (s0.x - s2.x) * (p.y - s2.y)) * invDet;
    float l2 = 1.0 - l0 - l1;

    float w0 = l0 / max(1e-5, p0.w);
    float w1 = l1 / max(1e-5, p1.w);
    float w2 = l2 / max(1e-5, p2.w);
    float sumW = w0 + w1 + w2;
    float3 bary = abs(sumW) > 1e-6 ? float3(w0, w1, w2) / sumW : float3(0.333, 0.333, 0.333);

    float3 worldNormal = normalize(
        mul((float3x3)pushConstants.model, v0.normal * bary.x + v1.normal * bary.y + v2.normal * bary.z)
    );

    float3 lightDir = normalize(float3(0.5, 1.0, 0.3));
    float diff = max(0.2, dot(worldNormal, lightDir));

    float3 color = float3(1.0);
    if (pushConstants.debugMode == 1) {
        // Per-meshlet color
        color = hashColor(meshletId + 1) * diff;
    } else if (pushConstants.debugMode == 2) {
        // Per-triangle color
        color = hashColor(triangleId + 1) * diff;
    } else if (pushConstants.debugMode == 3) {
        // World Normal view
        color = worldNormal * 0.5 + 0.5;
    } else if (pushConstants.debugMode == 4) {
        // Wireframe view
        float minBary = min(min(bary.x, bary.y), bary.z);
        float edge = smoothstep(0.0, 0.05, minBary);
        color = lerp(float3(1.0, 0.8, 0.1), hashColor(meshletId + 1) * diff, edge);
    } else {
        color = hashColor(meshletId + 1) * diff;
    }

    return float4(color, 1.0);
}
