#version 450

layout (location = 0) in vec3 dir;

layout (location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform samplerCube environmentMap;

void main()
{
    vec3 envColor = texture(environmentMap, dir).rgb;

    // Apply Reinhard tone mapping to compress HDR (high dynamic range) values to LDR (low dynamic range - monitor - [0,1])
    envColor = envColor / (envColor + vec3(1.0));

    outColor = vec4(envColor, 1.0f);
}
