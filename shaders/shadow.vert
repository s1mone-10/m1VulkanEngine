#version 450

layout (location = 0) in vec3 position;

layout(set = 0, binding = 1) uniform FrameUbo {
    mat4 view;
    mat4 proj;
    mat4 lightSpaceMatrix;
    vec3 camPos;
} frameUbo;

layout(push_constant) uniform Push {
    mat4 model;
    mat3 normalMatrix;
} push;

void main()
{
    gl_Position = frameUbo.lightSpaceMatrix * push.model * vec4(position, 1.0);
}
