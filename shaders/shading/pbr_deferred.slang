// pbr_deferred.slang
// Physically-based deferred shading resolve pass for visibility buffer with bindless texture arrays

#include "../common/brdf.slang"

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
    float4 cameraPos;
    float4 sunDirection; // xyz = dir towards sun, w = intensity
    float4 sunColor;     // xyz = light color, w = ambient intensity
    uint2 screenSize;
    uint debugMode;
    uint instanceCount;
};

[[vk::push_constant]]
DeferredPushConstants pushConstants;

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

struct GpuMaterialData {
    float4 baseColorFactor;
    float4 emissiveAndAlphaCutoff; // xyz = emissive, w = alphaCutoff
    float metallicFactor;
    float roughnessFactor;
    float normalScale;
    float occlusionStrength;
    uint baseColorTextureIndex;
    uint normalTextureIndex;
    uint metallicRoughnessTextureIndex;
    uint emissiveTextureIndex;
};

struct Vertex {
    float3 position;
    float3 normal;
    float2 uv;
};

// Set 0: Scene Buffers and Visibility/Depth Targets
[[vk::binding(0, 0)]] Texture2D<uint2> visibilityTexture;
[[vk::binding(1, 0)]] Texture2D<float> depthTexture;
[[vk::binding(2, 0)]] StructuredBuffer<Vertex> vertices;
[[vk::binding(3, 0)]] StructuredBuffer<uint> globalIndices;
[[vk::binding(4, 0)]] StructuredBuffer<GpuInstanceData> instances;
[[vk::binding(5, 0)]] StructuredBuffer<GpuMaterialData> materials;

// Set 1: Bindless Material Textures and Sampler
[[vk::binding(0, 1)]] SamplerState materialSampler;
[[vk::binding(1, 1)]] Texture2D textures[];

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
        float vignette = 1.0 - 0.3 * dot(ndc, ndc);
        float3 skyColor = lerp(float3(0.05, 0.07, 0.12), float3(0.02, 0.03, 0.05), input.uv.y);
        return float4(skyColor * vignette, 1.0);
    }

    uint triangleId = vis.x - 1;
    uint instanceId = (vis.y >> 16) > 0 ? ((vis.y >> 16) - 1) : 0;
    uint meshletId = vis.y & 0xFFFF;

    GpuInstanceData inst = instances[instanceId];
    GpuMaterialData mat = materials[inst.materialId];

    uint i0 = globalIndices[triangleId * 3 + 0];
    uint i1 = globalIndices[triangleId * 3 + 1];
    uint i2 = globalIndices[triangleId * 3 + 2];

    Vertex v0 = vertices[i0];
    Vertex v1 = vertices[i1];
    Vertex v2 = vertices[i2];

    float4 p0 = mul(pushConstants.viewProj, mul(inst.modelMatrix, float4(v0.position, 1.0)));
    float4 p1 = mul(pushConstants.viewProj, mul(inst.modelMatrix, float4(v1.position, 1.0)));
    float4 p2 = mul(pushConstants.viewProj, mul(inst.modelMatrix, float4(v2.position, 1.0)));

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

    // Interpolate geometric attributes
    float3 localPos = v0.position * bary.x + v1.position * bary.y + v2.position * bary.z;
    float3 worldPos = mul(inst.modelMatrix, float4(localPos, 1.0)).xyz;

    float3 localNormal = normalize(v0.normal * bary.x + v1.normal * bary.y + v2.normal * bary.z);
    float3 worldNormal = normalize(mul((float3x3)inst.normalMatrix, localNormal));

    float2 uv = v0.uv * bary.x + v1.uv * bary.y + v2.uv * bary.z;

    // Construct tangent space (TBN)
    float3 dp1 = v1.position - v0.position;
    float3 dp2 = v2.position - v0.position;
    float2 duv1 = v1.uv - v0.uv;
    float2 duv2 = v2.uv - v0.uv;

    float r = 1.0 / max(1e-6, abs(duv1.x * duv2.y - duv1.y * duv2.x));
    float3 tangent = normalize((dp1 * duv2.y - dp2 * duv1.y) * r);
    float3 worldTangent = normalize(mul((float3x3)inst.modelMatrix, tangent));
    worldTangent = normalize(worldTangent - worldNormal * dot(worldNormal, worldTangent));
    float3 worldBitangent = normalize(cross(worldNormal, worldTangent));

    // Sample bindless textures
    float4 sampledAlbedo = textures[mat.baseColorTextureIndex].Sample(materialSampler, uv);
    float3 albedo = sampledAlbedo.rgb * mat.baseColorFactor.rgb;

    float3 normalSample = textures[mat.normalTextureIndex].Sample(materialSampler, uv).rgb;
    float3 normalMap = normalSample * 2.0 - 1.0;
    normalMap.xy *= mat.normalScale;
    float3 finalNormal = normalize(worldTangent * normalMap.x + worldBitangent * normalMap.y + worldNormal * normalMap.z);

    // glTF metallic-roughness: green channel is roughness, blue channel is metallic
    float4 mrSample = textures[mat.metallicRoughnessTextureIndex].Sample(materialSampler, uv);
    float roughness = clamp(mrSample.g * mat.roughnessFactor, 0.04, 1.0);
    float metallic = clamp(mrSample.b * mat.metallicFactor, 0.0, 1.0);

    float3 emissive = textures[mat.emissiveTextureIndex].Sample(materialSampler, uv).rgb * mat.emissiveAndAlphaCutoff.xyz;
    float occlusion = 1.0;

    float3 viewDir = normalize(pushConstants.cameraPos.xyz - worldPos);

    PbrSurface surface;
    surface.albedo = albedo;
    surface.metallic = metallic;
    surface.roughness = roughness;
    surface.emissive = emissive;
    surface.occlusion = occlusion;
    surface.worldPos = worldPos;
    surface.worldNormal = finalNormal;
    surface.viewDir = viewDir;

    // Evaluate lighting
    float3 sunDir = normalize(pushConstants.sunDirection.xyz);
    float sunIntensity = pushConstants.sunDirection.w;
    float3 sunCol = pushConstants.sunColor.xyz;
    float ambientIntensity = pushConstants.sunColor.w;

    float3 directLight = evaluateDirectLight(surface, sunDir, sunCol, sunIntensity);
    float3 ambientLight = evaluateAmbientLight(surface, sunCol, ambientIntensity);
    float3 finalColor = directLight + ambientLight;

    // Debug Modes
    if (pushConstants.debugMode == 1) {
        // Meshlet IDs
        finalColor = hashColor(meshletId + 1);
    } else if (pushConstants.debugMode == 2) {
        // Triangle IDs
        finalColor = hashColor(triangleId + 1);
    } else if (pushConstants.debugMode == 3) {
        // World Normals
        finalColor = finalNormal * 0.5 + 0.5;
    } else if (pushConstants.debugMode == 4) {
        // Wireframe
        float minBary = min(min(bary.x, bary.y), bary.z);
        float edge = smoothstep(0.0, 0.03, minBary);
        finalColor = lerp(float3(1.0, 0.8, 0.1), finalColor, edge);
    } else if (pushConstants.debugMode == 5) {
        // Instance IDs
        finalColor = hashColor(instanceId + 1);
    } else if (pushConstants.debugMode == 6) {
        // Base Color / Albedo
        finalColor = albedo;
    } else if (pushConstants.debugMode == 7) {
        // Roughness (R), Metallic (B)
        finalColor = float3(roughness, roughness * metallic, metallic);
    } else {
        // Full PBR lit tone mapping + sRGB output
        finalColor = linearToSrgb(acesFilmic(finalColor));
    }

    return float4(finalColor, 1.0);
}
