#version 450

struct Light {
    vec4 position; // w=0 directional, w=1 point
    vec4 color;    // rgb = color, a = intensity
};

layout (location = 0) in vec3 fragColor;
layout (location = 1) in vec2 fragTexCoord;
layout (location = 2) in vec3 fragPos;
layout (location = 3) in vec3 fragNormal;

// specify the out location (index of the framebuffer attachment) and out variable
layout (location = 0) out vec4 outColor;

layout (binding = 1) uniform sampler2D texSampler;

layout(binding = 2) uniform LightsUbo {
    vec4 ambient; // rgb = ambient color, a = intensity
    Light lights[10];
    int numLights;
} lightsUbo;


//layout(push_constant) uniform Push {
//    vec2 offset;
//    vec3 color;
//} push;

void main(){
    //outColor = vec4(fragColor, 1.0); // rgba color, range [0, 1]
    //outColor = vec4(fragTexCoord, 0.0, 1.0);
    //outColor = vec4(push.color, 1.0);
    
    //outColor = texture(texSampler, fragTexCoord);
    //return;

    // lighting
    vec3 norm = normalize(fragNormal);
    Light light = lightsUbo.lights[0];
    vec3 lightDir = (light.position.w == 0.0)
                 ? normalize(-light.position.xyz)  // directional
                 : normalize(light.position.xyz - fragPos);
                 
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * light.color.rgb * light.color.a;
    vec3 ambient = lightsUbo.ambient.rgb * lightsUbo.ambient.a;


    outColor = vec4(fragColor * (ambient + diffuse), 1.0);
}