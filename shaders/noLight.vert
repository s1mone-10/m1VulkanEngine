#version 450 // Specifies the GLSL version

// Input
layout (location = 0) in vec3 position;
layout (location = 1) in vec3 color;

// Output
layout (location = 0) out vec3 fragColor;

// Frame UBO
layout(set = 0, binding = 1) uniform FrameUbo {
    mat4 view;
    mat4 proj;
    mat4 lightViewProjMatrix;
    vec4 camPos;
    int shadowsEnabled;
} frameUbo;

// Push Constants
layout(push_constant) uniform Push {
    mat4 model;
    mat3 normalMatrix;
} push;

void main() {
    // gl_Position is a built-in output variable that stores the final vertex position in the vertex shader
    // Sets the final vertex position in clip space (range [-w, +w] for x, y, z. GPU uses them for clipping against the view frustum before perspective division to NDC.)
    // The x,y,z values will be converted in Normalized Device Coordinates (NDC) (range [-1, 1]) by dividing them by w
    gl_Position = frameUbo.proj * frameUbo.view * push.model * vec4(position, 1.0);

    fragColor = color; // Pass the color to the fragment shader
}