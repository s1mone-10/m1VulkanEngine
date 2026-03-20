#version 450

layout (location = 0) in vec3 dir;

layout (location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform samplerCube cubeSampler;

void main()
{
    outColor = texture(cubeSampler, dir);
}
