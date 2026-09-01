// brdf.slang
// Physically-Based Rendering Cook-Torrance Microfacet BRDF and lighting evaluation

static const float PI = 3.14159265358979323846;
static const float INV_PI = 0.31830988618379067154;

// Trowbridge-Reitz GGX Normal Distribution Function
float distributionGGX(float NdotH, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH2 = NdotH * NdotH;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    return a2 / (PI * denom * denom + 1e-7);
}

// Schlick-GGX Geometric Masking-Shadowing for a single direction
float geometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;
    return NdotV / (NdotV * (1.0 - k) + k + 1e-7);
}

// Smith joint masking-shadowing function
float geometrySmith(float NdotV, float NdotL, float roughness) {
    float ggx2 = geometrySchlickGGX(NdotV, roughness);
    float ggx1 = geometrySchlickGGX(NdotL, roughness);
    return ggx1 * ggx2;
}

// Fresnel-Schlick approximation
float3 fresnelSchlick(float cosTheta, float3 F0) {
    return F0 + (float3(1.0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// Fresnel-Schlick with roughness for ambient terms
float3 fresnelSchlickRoughness(float cosTheta, float3 F0, float roughness) {
    return F0 + (max(float3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

struct PbrSurface {
    float3 albedo;
    float metallic;
    float roughness;
    float3 emissive;
    float occlusion;
    float3 worldPos;
    float3 worldNormal;
    float3 viewDir;
};

// Evaluates standard Cook-Torrance direct directional light
float3 evaluateDirectLight(PbrSurface surface, float3 lightDir, float3 lightColor, float lightIntensity) {
    float3 N = surface.worldNormal;
    float3 V = surface.viewDir;
    float3 L = normalize(lightDir);
    float3 H = normalize(V + L);

    float NdotV = max(dot(N, V), 1e-4);
    float NdotL = max(dot(N, L), 0.0);
    float NdotH = max(dot(N, H), 0.0);
    float VdotH = max(dot(V, H), 0.0);

    // Dielectrics have base reflectivity ~0.04, metals use their albedo color
    float3 F0 = lerp(float3(0.04), surface.albedo, surface.metallic);

    float D = distributionGGX(NdotH, surface.roughness);
    float G = geometrySmith(NdotV, NdotL, surface.roughness);
    float3 F = fresnelSchlick(VdotH, F0);

    float3 numerator = D * G * F;
    float denominator = 4.0 * NdotV * NdotL + 1e-4;
    float3 specular = numerator / denominator;

    // Energy conservation
    float3 kS = F;
    float3 kD = (float3(1.0) - kS) * (1.0 - surface.metallic);

    float3 diffuse = kD * surface.albedo * INV_PI;

    float3 radiance = lightColor * lightIntensity;
    return (diffuse + specular) * radiance * NdotL;
}

// Evaluates ambient lighting for both dielectrics and metals
float3 evaluateAmbientLight(PbrSurface surface, float3 ambientColor, float ambientIntensity) {
    float3 F0 = lerp(float3(0.04), surface.albedo, surface.metallic);
    float3 kS = fresnelSchlickRoughness(max(dot(surface.worldNormal, surface.viewDir), 0.0), F0, surface.roughness);
    float3 kD = (float3(1.0) - kS) * (1.0 - surface.metallic);

    float3 diffuse = surface.albedo * ambientColor * ambientIntensity * surface.occlusion;
    float3 specular = ambientColor * ambientIntensity * kS * surface.occlusion;
    return (diffuse * kD + specular) + surface.emissive;
}

// ACES filmic tone mapping curve (linear in, linear out in [0, 1])
float3 acesFilmic(float3 x) {
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

// Linear to standard sRGB gamma encoding
float3 linearToSrgb(float3 color) {
    return pow(max(color, float3(0.0)), float3(1.0 / 2.2));
}
