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

// Input
layout (location = 0) in vec3 fragColor;
layout (location = 1) in vec2 fragTextCoord;
layout (location = 2) in vec3 fragPosWorld;
layout (location = 3) in vec3 fragNormalWorld;

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
    vec3 camPos;
} frameUbo;

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
LightComponents calculateLight(Light light, vec3 fragNormal, vec3 diffuseColor, vec3 specularColor);

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

    // loops on the lights to get diffuse and specular components
    vec3 diffuseComponent = vec3(0.0);
    vec3 specularComponent = vec3(0.0);
    for (int i = 0; i < lightsUbo.numLights; i++) {
        LightComponents lc = calculateLight(lightsUbo.lights[i], fragNormal, diffuseColor, specularColor);
        diffuseComponent += lc.diffuse;
        specularComponent += lc.specular;
    }

    // comput the ambient component
    vec3 ambientComponent = ambientColor * lightsUbo.ambient.rgb * lightsUbo.ambient.a;

    // sum lights components
    outColor = vec4((ambientComponent + diffuseComponent + specularComponent), 1.0);
}

LightComponents calculateLight(Light light, vec3 fragNormal, vec3 diffuseColor, vec3 specularColor) {
    vec3 lightDir = (light.posDir.w == 0.0)
                    ? normalize(-light.posDir.xyz)  // directional
                    : normalize(light.posDir.xyz - fragPosWorld); // point

    // multiply color for intensity
    vec3 lightColor = light.color.rgb * light.color.a;

    // compute attenuation for point lights (1 / (constant + linear*distance + quadratic*distance^2))
    if (light.posDir.w == 1.0) {
        float dist = length(light.posDir.xyz - fragPosWorld);
        float attenuation = 1.0 / (light.attenuation.x + light.attenuation.y * dist + light.attenuation.z * dist * dist);
        lightColor *= attenuation;
    }

    // diffuse strength of the light by taking dot product between frag normal and light direction
    // (max function because if the angle > 90 degrees the value will be negative)
    float diffStrength = max(dot(fragNormal, lightDir), 0.0);

    // final diffuse color component
    vec3 diffuseComponent = diffuseColor * lightColor * diffStrength;

    vec3 viewDir = normalize(frameUbo.camPos - fragPosWorld);

    // calculate the reflection vector by reflecting the light direction around the normal vector
    vec3 reflectDir = reflect(-lightDir, fragNormal);

    // specular strength: raise to the power of shininess the dot product between view direction and reflection direction
    float specStrength = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);

    // final specular color component
    vec3 specularComponent = specularColor * lightColor * specStrength;

    return LightComponents(diffuseComponent, specularComponent);
}