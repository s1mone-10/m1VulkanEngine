#version 450 // Specifies the GLSL version

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

layout (location = 0) in vec3 position;
layout (location = 1) in vec3 color;
layout(location = 2) in vec2 texCoord;

layout (location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;

void main() {

    // gl_Position is a built-in output variable that stores the final vertex position in the vertex shader
    // Sets the final vertex position in clip space (range [-w, +w] for x, y, z. GPU uses them for clipping against the view frustum before perspective division to NDC.)
    // The x,y,z values will be converted in Normalized Device Coordinates (NDC) (range [-1, 1]) by dividing them by w
    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(position, 1.0);

    fragColor = color; // Pass the color to the fragment shader
    fragTexCoord = texCoord;
}