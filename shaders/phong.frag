#version 450

struct Light {
    vec4 posDir;        // w=0 directional, w=1 point
    vec4 color;         // rgb = color, a = intensity
    vec4 attenuation;   // x = constant, y = linear, z = quadratic
};

struct LightComponents {
    vec3 diffuse;
    vec3 specular;
};

layout (location = 0) in vec3 fragColor;
layout (location = 1) in vec2 fragTextCoord;
layout (location = 2) in vec3 fragPosWorld;
layout (location = 3) in vec3 fragNormalWorld;
layout (location = 4) in vec4 fragPosLightSpace;

layout (location = 0) out vec4 outColor;

layout(set = 0, binding = 2) uniform LightsUbo {
    vec4 ambient;
    Light lights[10];
    int numLights;
} lightsUbo;

layout(set = 0, binding = 1) uniform FrameUbo {
    mat4 view;
    mat4 proj;
    mat4 lightSpaceMatrix;
    vec3 camPos;
} frameUbo;

layout(set = 0, binding = 5) uniform sampler2D shadowMap;

layout (set = 1, binding = 0) uniform MaterialUbo {
    float shininess;
    float opacity;
    vec3 diffuseColor;
    vec3 specularColor;
    vec3 ambientColor;
} material;

layout (set = 1, binding = 1) uniform sampler2D diffuseMap;
layout (set = 1, binding = 2) uniform sampler2D specularMap;

layout(push_constant) uniform Push {
    mat4 model;
    mat3 normalMatrix;
} push;

LightComponents calculateLight(Light light, vec3 fragNormal, vec3 diffuseColor, vec3 specularColor);

float calculateShadow(vec4 fragPosLightSpace, vec3 normal, vec3 lightDir)
{
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;

    if (projCoords.z > 1.0 || projCoords.x < 0.0 || projCoords.x > 1.0 || projCoords.y < 0.0 || projCoords.y > 1.0)
        return 0.0;

    float closestDepth = texture(shadowMap, projCoords.xy).r;
    float currentDepth = projCoords.z;
    float bias = max(0.003 * (1.0 - dot(normal, lightDir)), 0.0005);
    return currentDepth - bias > closestDepth ? 1.0 : 0.0;
}

void main(){
    vec3 diffuseColor = texture(diffuseMap, fragTextCoord).rgb * fragColor * material.diffuseColor;
    vec3 ambientColor = texture(diffuseMap, fragTextCoord).rgb * fragColor * material.ambientColor;
    vec3 specularColor = texture(specularMap, fragTextCoord).rgb * material.specularColor;

    vec3 fragNormal = normalize(fragNormalWorld);

    vec3 diffuseComponent = vec3(0.0);
    vec3 specularComponent = vec3(0.0);
    for (int i = 0; i < lightsUbo.numLights; i++) {
        LightComponents lc = calculateLight(lightsUbo.lights[i], fragNormal, diffuseColor, specularColor);
        diffuseComponent += lc.diffuse;
        specularComponent += lc.specular;
    }

    vec3 ambientComponent = ambientColor * lightsUbo.ambient.rgb * lightsUbo.ambient.a;

    vec3 directionalLightDir = normalize(-lightsUbo.lights[1].posDir.xyz);
    float shadow = calculateShadow(fragPosLightSpace, fragNormal, directionalLightDir);

    vec3 lit = ambientComponent + (1.0 - shadow) * (diffuseComponent + specularComponent);
    outColor = vec4(lit, 1.0);
}

LightComponents calculateLight(Light light, vec3 fragNormal, vec3 diffuseColor, vec3 specularColor) {
    vec3 lightDir = (light.posDir.w == 0.0)
                    ? normalize(-light.posDir.xyz)
                    : normalize(light.posDir.xyz - fragPosWorld);

    vec3 lightColor = light.color.rgb * light.color.a;

    if (light.posDir.w == 1.0) {
        float dist = length(light.posDir.xyz - fragPosWorld);
        float attenuation = 1.0 / (light.attenuation.x + light.attenuation.y * dist + light.attenuation.z * dist * dist);
        lightColor *= attenuation;
    }

    float diffStrength = max(dot(fragNormal, lightDir), 0.0);
    vec3 diffuseComponent = diffuseColor * lightColor * diffStrength;

    vec3 viewDir = normalize(frameUbo.camPos - fragPosWorld);
    vec3 reflectDir = reflect(-lightDir, fragNormal);
    float specStrength = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
    vec3 specularComponent = specularColor * lightColor * specStrength;

    return LightComponents(diffuseComponent, specularComponent);
}
