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
    mat4 lightSpaceMatrix;
    vec3 camPos;
} frameUbo;

layout(push_constant) uniform Push {
    mat4 model;
    mat3 normalMatrix;
} push;

void main() {
    gl_Position = frameUbo.proj * frameUbo.view * push.model * vec4(position, 1.0);

    fragColor = color;
    fragTexCoord = texCoord;
    fragPosWorld = vec3(push.model * vec4(position, 1.0));
    fragNormalWorld = normalize(push.normalMatrix * normal);
    fragPosLightSpace = frameUbo.lightSpaceMatrix * vec4(fragPosWorld, 1.0);
}
