#version 450

// Input
layout (location = 0) in vec2 position;
layout (location = 1) in vec4 color;

// Output
layout (location = 0) out vec3 fragColor;

layout(set = 0, binding = 1) uniform FrameUbo {
    mat4 view;
    mat4 proj;
    mat4 lightViewProjMatrix;
    vec4 camPos;
    int shadowsEnabled;
} frameUbo;

void main() {
    gl_PointSize = 2.0;
    gl_Position = frameUbo.proj * frameUbo.view * vec4(position.xy, 1.0, 1.0);

    fragColor = color.rgb;
}