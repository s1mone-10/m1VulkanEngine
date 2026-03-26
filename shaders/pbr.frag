#version 450

struct Light {
    vec4 posDir;// w=0 directional, w=1 point
    vec4 color;// rgb = color, a = intensity
    vec4 attenuation;// x = constant, y = linear, z = quadratic
};

const float PI = 3.14159265359;

// Input
layout (location = 0) in vec3 fragColor;
layout (location = 1) in vec2 fragTextCoord;
layout (location = 2) in vec3 fragPosWorld;
layout (location = 3) in vec4 fragPosLightSpace;
layout (location = 4) in mat3 TBN;// Tangent-Bitangent-Normal matrix for normal mapping

// Output. Specify the out location (index of the framebuffer attachment) and out variable
layout (location = 0) out vec4 outColor;

// === SET 0 ===
layout(set = 0, binding = 1) uniform FrameUbo {
    mat4 view;
    mat4 proj;
    mat4 lightViewProjMatrix;
    vec4 camPos;
    float iblIntensity;
    int shadowsEnabled;
} frameUbo;

layout(set = 0, binding = 2) uniform LightsUbo {
    vec4 ambient;// rgb = ambient color, a = intensity
    Light lights[10];
    int numLights;
} lightsUbo;

layout (set = 0, binding = 3) uniform sampler2D shadowMap;
layout (set = 0, binding = 4) uniform samplerCube irradianceMap;
layout (set = 0, binding = 5) uniform samplerCube prefilteredMap;
layout (set = 0, binding = 6) uniform sampler2D brdfLUT;

// === SET 1 ===
layout (set = 1, binding = 0) uniform MaterialUbo {
    vec4 baseColor;
    vec4 emissiveFactor;
    float metallicFactor;
    float roughnessFactor;
} material;

// pbr maps samplers
layout (set = 1, binding = 1) uniform sampler2D albedoMap;
layout (set = 1, binding = 2) uniform sampler2D normalMap;
layout (set = 1, binding = 3) uniform sampler2D metallicRoughnessMap;
layout (set = 1, binding = 4) uniform sampler2D aoMap;// ambient occlusion
layout (set = 1, binding = 5) uniform sampler2D emissiveMap;// Lets materials glow independent of lighting (e.g., LEDs, screens)

// Push constant
layout(push_constant) uniform Push {
    mat4 model;
    mat3 normalMatrix;
} push;

// Normal Distribution Function (D) - GGX/Trowbridge-Reitz Distribution
// Approximates the amount the surface's microfacets are aligned to the halfway vector
float DistributionGGX(float NdotH, float roughness) {
    float a = roughness * roughness;// Remapping for more perceptual linearity
    float a2 = a * a;
    float NdotH2 = NdotH * NdotH;

    float num = a2;// Numerator: concentration factor
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;// Normalization factor

    return num / denom;// Normalized distribution
}

// Geometry Function (G) - Smith's method with Schlick-GGX approximation
// Approximate self-shadowing between microfacets. Microfacets can overshadow other microfacets reducing the light the surface reflects.
float GeometrySmith(float NdotV, float NdotL, float roughness) {
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;// Direct lighting remapping

    // Geometry obstruction from view direction (masking)
    float ggx1 = NdotV / (NdotV * (1.0 - k) + k);
    // Geometry obstruction from light direction (shadowing)
    float ggx2 = NdotL / (NdotL * (1.0 - k) + k);

    return ggx1 * ggx2;// Combined masking-shadowing
}

// Fresnel Reflectance (F) - Schlick's approximation
// Compute the ratio of light that gets reflected over the light that gets refracted.
// F0 parameter is the surface reflection at zero incidence (how much the surface reflects if looking directly at the surface)
vec3 FresnelSchlick(float cosTheta, vec3 F0, float roughness)
{
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 calculateLight(Light light, vec3 N, vec3 baseColor, vec3 V, vec3 F0, float metallic, float roughness, vec2 texelSize);
float calculateShadow(vec4 fragPosLightSpace, vec3 normal, vec3 lightDir, vec2 texelSize);

void main(){

    // TODO optimizaion?: don't transform the normal but light variable in tangent space in the vertex shader (see learnOpengl)
    // Sample normal map and convert from [0,1] to [-1,1] range
    vec3 N = texture(normalMap, fragTextCoord).xyz * 2.0 - 1.0;
    // Transform normal from tangent space to world space
    N = normalize(TBN * N); // TODO TBN matrix must be re-orthogonalized or normalized?

    // Calculate view direction (fragment to camera)
    vec3 V = normalize(frameUbo.camPos.xyz - fragPosWorld);

    // Calculate reflection vector for environment mapping
    vec3 R = reflect(-V, N);

    // ========== TEXTURE SAMPLING =========

    // get the base color by multiply texture, vertex and material colors
    // Use white (1,1,1) as default so missing texture / vertex / material color does not affect the result
    vec4 baseColor = texture(albedoMap, fragTextCoord) * vec4(fragColor, 1) * material.baseColor;

    // glTF metallic-roughness texture packs metallic in B, roughness in G (linear space)
    vec4 metallicRoughness = texture(metallicRoughnessMap, fragTextCoord);
    float metallic = clamp(metallicRoughness.b * material.metallicFactor, 0.0, 1.0);
    float roughness = clamp(metallicRoughness.g * material.roughnessFactor, 0.0, 1.0);

    // ambient occlusion
    float ao = texture(aoMap, fragTextCoord).r;

    // emissive color
    vec3 emissive = texture(emissiveMap, fragTextCoord).rgb * material.emissiveFactor.rgb;

    // Calculate F0 (reflectance at normal incidence)
    // Non-metals: default low reflectance (~0.04), Metals: colored reflectance from base color
    // linear interpolation between default 0.04 and the albedo color using metallic property to weight between them.
    vec3 F0 = mix(vec3(0.04), baseColor.rgb, metallic);

    // get the size of one texel in texture space (used for PCF in shadow calculation)
    vec2 texelSize = 1.0 / textureSize(shadowMap, 0);

    // Initialize outgoing radiance accumulator
    vec3 Lo = vec3(0.0);

    // light loop to accumulate radiance from each light source
    for (int i = 0; i < lightsUbo.numLights; i++) {
        Lo += calculateLight(lightsUbo.lights[i], N, baseColor.rgb, V, F0, metallic, roughness, texelSize);
    }

    // ============  IBL - ambient light ===================
    float NdotV = max(dot(N, V), 0.0); // View angle
    vec3 kS = FresnelSchlick(NdotV, F0, roughness);
    vec3 kD = 1.0 - kS;
    kD *= 1.0 - metallic;

    vec3 irradiance = texture(irradianceMap, N).rgb;
    vec3 diffuse    = irradiance * baseColor.rgb;

    const float MAX_REFLECTION_LOD = 4.0; // max mip level index of the texture. 5 mip levels -> (0 to 4)
    vec3 prefilteredColor = textureLod(prefilteredMap, R,  roughness * MAX_REFLECTION_LOD).rgb;
    vec2 envBRDF  = texture(brdfLUT, vec2(NdotV, roughness)).rg;
    vec3 specular = prefilteredColor * (kS * envBRDF.x + envBRDF.y);

    vec3 ambient = (kD * diffuse + specular) * frameUbo.iblIntensity * ao;

    // Combine all lighting contributions
    vec3 color = ambient + Lo + emissive;

    // Apply Reinhard tone mapping to compress HDR (high dynamic range) values to LDR (low dynamic range - monitor - [0,1])
    color = color / (color + vec3(1.0));

    // Output final color with original alpha
    outColor = vec4(color, baseColor.a);
}

vec3 calculateLight(Light light, vec3 N, vec3 baseColor, vec3 V, vec3 F0, float metallic, float roughness, vec2 texelSize) {
    // Light direction
    vec3 L = (light.posDir.w == 0.0)
    ? normalize(-light.posDir.xyz)// directional
    : normalize(light.posDir.xyz - fragPosWorld);// point

    // Half vector (between view and light directions)
    vec3 H = normalize(V + L);

    // 1 => object not in shadow
    float shadow = 1;// TODO - use shadow?

    // multiply color for intensity
    vec3 radiance = light.color.rgb * light.color.a;

    if (light.posDir.w == 1.0) {
        // compute attenuation for point lights (1 / (constant + linear*distance + quadratic*distance^2))
        float dist = length(light.posDir.xyz - fragPosWorld);
        float attenuation = 1.0 / (light.attenuation.x + light.attenuation.y * dist + light.attenuation.z * dist * dist);
        radiance *= attenuation;
    }
    else if (frameUbo.shadowsEnabled == 1) {
        // compute shadow for directional light
        shadow = calculateShadow(fragPosLightSpace, N, L, texelSize);
    }

    // === BRDF EVALUATION ===
    // Calculate all necessary dot products for BRDF terms
    float NdotL = max(dot(N, L), 0.0);// Lambertian falloff
    float NdotV = max(dot(N, V), 0.0);// View angle
    float NdotH = max(dot(N, H), 0.0);// Half vector for specular
    float HdotV = max(dot(H, V), 0.0);// For Fresnel calculation

    // Evaluate Cook-Torrance BRDF components
    float D = DistributionGGX(NdotH, roughness);// Normal distribution
    float G = GeometrySmith(NdotV, NdotL, roughness);// Geometry function
    vec3 F = FresnelSchlick(HdotV, F0, 0);// Fresnel reflectance

    // Calculate specular BRDF
    vec3 numerator = D * G * F;
    float denominator = 4.0 * NdotV * NdotL + 0.0001;// Prevent division by zero
    vec3 specular = numerator / denominator;

    // === ENERGY CONSERVATION ===
    // Fresnel term represents specular reflection ratio
    vec3 kS = F;// Specular contribution
    vec3 kD = vec3(1.0) - kS;// Diffuse contribution (energy conservation)

    kD *= 1.0 - metallic; // Metals have no diffuse reflection

    // === RADIANCE ACCUMULATION ===
    // Combine diffuse (Lambertian) and specular (Cook-Torrance) terms
    // Multiply by incident radiance and cosine foreshortening
    return (kD * baseColor / PI + specular) * radiance * NdotL;
}

float calculateShadow(vec4 fragPosLightSpace, vec3 normal, vec3 lightDir, vec2 texelSize)
{
    // convert in Normalized Device Coordinates
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;

    // convert to range [0, 1] for sampling the texture (z is already in range [0, 1]
    projCoords.xy = projCoords.xy * 0.5 + 0.5;

    // coordinate is further than the light's far plane are not in shadow
    if (projCoords.z > 1.0)
    return 1.0;

    float currentDepth = projCoords.z;

    // [0.005, 0.0005] bias to prevent shadow acne
    float bias = max(0.005 * (1.0 - dot(normal, lightDir)), 0.0005);

    // PCF (Percentage Closer Filtering) for softer shadow edges
    // sample the shadow map around the projected coordinate and average the result
    float shadow = 0.0;
    for (int x = -1; x <= 1; ++x)
    {
        for (int y = -1; y <= 1; ++y)
        {
            float pcfDepth = texture(shadowMap, projCoords.xy + vec2(x, y) * texelSize).r;
            shadow += currentDepth - bias < pcfDepth ? 1.0 : 0.0;
        }
    }
    shadow /= 9.0;// average the 9 samples

    return shadow;
}