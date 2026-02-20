#version 450

struct Light {
    vec4 posDir;        // w=0 directional, w=1 point
    vec4 color;         // rgb = color, a = intensity
    vec4 attenuation;   // x = constant, y = linear, z = quadratic
};

// Input
layout (location = 0) in vec3 fragColor;
layout (location = 1) in vec2 fragTextCoord;
layout (location = 2) in vec3 fragPosWorld;
layout (location = 3) in vec3 fragNormalWorld;
layout (location = 4) in vec4 fragPosLightSpace;

// Output. Specify the out location (index of the framebuffer attachment) and out variable
layout (location = 0) out vec4 outColor;

// Lights ubo
layout(set = 0, binding = 2) uniform LightsUbo {
    vec4 ambient; // rgb = ambient color, a = intensity
    Light lights[10];
    int numLights;
} lightsUbo;

// Frame ubo
layout(set = 0, binding = 1) uniform FrameUbo {
    mat4 view;
    mat4 proj;
    mat4 lightViewProjMatrix;
    vec4 camPos;
    int shadowsEnabled;
} frameUbo;

// shadow map sampler
layout(set = 0, binding = 5) uniform sampler2D shadowMap;

// Material ubo
layout (set = 1, binding = 0) uniform MaterialUbo {
    float shininess;
    float opacity;
    vec3 diffuseColor;
    vec3 specularColor;
    vec3 ambientColor;
} material;

// diffuse map sampler
layout (set = 1, binding = 1) uniform sampler2D diffuseMap;

// specular map sampler
layout (set = 1, binding = 2) uniform sampler2D specularMap;

// Push constant
layout(push_constant) uniform Push {
    mat4 model;
    mat3 normalMatrix;
} push;

// Functions
vec3 calculateLight(Light light, vec3 fragNormal, vec3 diffuseColor, vec3 specularColor, vec2 texelSize);
float calculateShadow(vec4 fragPosLightSpace, vec3 normal, vec3 lightDir, vec2 texelSize);

void main(){
    //outColor = vec4(fragColor, 1.0); // rgba color, range [0, 1]
    //outColor = vec4(fragTexCoord, 0.0, 1.0);
    //outColor = vec4(push.color, 1.0);
    
    //outColor = texture(texSampler, fragTexCoord);
    //return;

    // get the color by multiply texture, vertex and material colors
    // Use white (1,1,1) as default so missing texture / vertex / material color does not affect the result
    vec3 diffuseColor = texture(diffuseMap, fragTextCoord).rgb * fragColor * material.diffuseColor;
    vec3 ambientColor = texture(diffuseMap, fragTextCoord).rgb * fragColor * material.ambientColor;
    vec3 specularColor = texture(specularMap, fragTextCoord).rgb * material.specularColor;

    // normalize the frag normal
    vec3 fragNormal = normalize(fragNormalWorld);

    // get the size of one texel in texture space (used for PCF in shadow calculation)
    vec2 texelSize = 1.0 / textureSize(shadowMap, 0);

    // loops on the lights to get diffuse and specular components
    vec3 diffuseAndSpecularComponent = vec3(0.0);
    for (int i = 0; i < lightsUbo.numLights; i++) {
        diffuseAndSpecularComponent += calculateLight(lightsUbo.lights[i], fragNormal, diffuseColor, specularColor, texelSize);
    }

    // comput the ambient component
    vec3 ambientComponent = ambientColor * lightsUbo.ambient.rgb * lightsUbo.ambient.a;

    // sum lights components
    outColor = vec4((ambientComponent + diffuseAndSpecularComponent), 1.0);
}

vec3 calculateLight(Light light, vec3 fragNormal, vec3 diffuseColor, vec3 specularColor, vec2 texelSize) {
    vec3 lightDir = (light.posDir.w == 0.0)
                    ? normalize(-light.posDir.xyz)  // directional
                    : normalize(light.posDir.xyz - fragPosWorld); // point

    // 1 => object not in shadow
    float shadow = 1;

    // multiply color for intensity
    vec3 lightColor = light.color.rgb * light.color.a;

    if (light.posDir.w == 1.0) {
        // compute attenuation for point lights (1 / (constant + linear*distance + quadratic*distance^2))
        float dist = length(light.posDir.xyz - fragPosWorld);
        float attenuation = 1.0 / (light.attenuation.x + light.attenuation.y * dist + light.attenuation.z * dist * dist);
        lightColor *= attenuation;
    }
    else if (frameUbo.shadowsEnabled == 1) {
        // compute shadow for directional light
        shadow = calculateShadow(fragPosLightSpace, fragNormal, lightDir, texelSize);
    }

    // diffuse strength of the light by taking dot product between frag normal and light direction
    // (max function because if the angle > 90 degrees the value will be negative)
    float diffStrength = max(dot(fragNormal, lightDir), 0.0);

    // final diffuse color component
    vec3 diffuseComponent = diffuseColor * lightColor * diffStrength;

    vec3 viewDir = normalize(frameUbo.camPos.xyz - fragPosWorld);

    // calculate the reflection vector by reflecting the light direction around the normal vector
    vec3 reflectDir = reflect(-lightDir, fragNormal);

    // specular strength: raise to the power of shininess the dot product between view direction and reflection direction
    float specStrength = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);

    // final specular color component
    vec3 specularComponent = specularColor * lightColor * specStrength;

    return (diffuseComponent + specularComponent) * shadow;
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
    for(int x = -1; x <= 1; ++x)
    {
        for(int y = -1; y <= 1; ++y)
        {
            float pcfDepth = texture(shadowMap, projCoords.xy + vec2(x, y) * texelSize).r;
            shadow += currentDepth - bias < pcfDepth ? 1.0 : 0.0;
        }
    }
    shadow /= 9.0; // average the 9 samples

    return shadow;
}