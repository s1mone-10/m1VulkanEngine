#version 450 // Specifies the GLSL version

// Input
layout (location = 0) in vec3 position;
layout (location = 1) in vec3 color;
layout (location = 2) in vec3 normal;
layout (location = 3) in vec2 texCoord;

// Output
layout (location = 0) out vec3 fragColor;
layout (location = 1) out vec2 fragTexCoord;
layout (location = 2) out vec3 fragPosWorld;
layout (location = 3) out vec3 fragNormalWorld;
layout (location = 4) out vec4 fragPosLightSpace;

layout(set = 0, binding = 0) uniform ObjectUbo {
    mat4 model;
    mat3 normalMatrix;
} objectUbo;

layout(set = 0, binding = 1) uniform FrameUbo {
    mat4 view;
    mat4 proj;
    mat4 lightViewProjMatrix;
    vec4 camPos;
    int shadowsEnabled;
} frameUbo;

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
    fragTexCoord = texCoord;
    fragPosWorld = vec3(push.model * vec4(position, 1.0));
    fragNormalWorld = normalize(push.normalMatrix * normal);
    fragPosLightSpace = frameUbo.lightViewProjMatrix * vec4(fragPosWorld, 1.0);
}