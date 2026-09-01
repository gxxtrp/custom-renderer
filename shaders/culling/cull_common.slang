// cull_common.slang
// Common definitions and geometry intersection tests for GPU compute culling

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

struct MeshletDrawTask {
    uint instanceId;
    uint meshletId;
};

struct DrawMeshTasksIndirectCommand {
    uint groupCountX;
    uint groupCountY;
    uint groupCountZ;
};

// Tests a sphere against 6 frustum planes (Ax + By + Cz + D = 0, inward-pointing normals)
bool isSphereInsideFrustum(float3 center, float radius, float4 planes[6]) {
    [unroll]
    for (int i = 0; i < 6; ++i) {
        if (dot(planes[i].xyz, center) + planes[i].w < -radius) {
            return false;
        }
    }
    return true;
}

// Tests an axis-aligned bounding box against 6 frustum planes
bool isAabbInsideFrustum(float3 boxMin, float3 boxMax, float4 planes[6]) {
    [unroll]
    for (int i = 0; i < 6; ++i) {
        float3 p = float3(
            planes[i].x >= 0.0 ? boxMax.x : boxMin.x,
            planes[i].y >= 0.0 ? boxMax.y : boxMin.y,
            planes[i].z >= 0.0 ? boxMax.z : boxMin.z
        );
        if (dot(planes[i].xyz, p) + planes[i].w < 0.0) {
            return false;
        }
    }
    return true;
}

// Evaluates normal cone against camera position for backface rejection.
// Uses meshoptimizer conservative bounding sphere formula (Real-Time Rendering 4th Ed §19.3):
// dot(center - camera_pos, cone_axis) >= cone_cutoff * length(center - camera_pos) + radius
// A cluster can only be backface culled as a whole if all its normals lie strictly within a single hemisphere
// (cone angle < 180 degrees, coneCutoff > 0). Clusters with coneCutoff <= 0 wrap around and are never culled as a whole.
bool isConeBackfacing(float3 worldCenter, float worldRadius, float3 worldConeAxis, float coneCutoff, float3 cameraPos) {
    if (coneCutoff <= 0.0 || coneCutoff >= 1.0) {
        return false;
    }
    float3 viewDir = worldCenter - cameraPos;
    float dist = length(viewDir);
    if (dist <= worldRadius) {
        // Camera is within the bounding sphere: do not cull
        return false;
    }
    return dot(viewDir, worldConeAxis) >= (coneCutoff * dist + worldRadius);
}
