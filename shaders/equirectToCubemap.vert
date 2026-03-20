#version 450

layout (location = 0) in vec3 position;

layout (location = 0) out vec3 localPos;

layout(push_constant) uniform Push {
    mat4 projection;
    mat4 view;
} push;

void main()
{
    localPos = position;
    gl_Position =  push.projection * push.view * vec4(position, 1.0);
}
