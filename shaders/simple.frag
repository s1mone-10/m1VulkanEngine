#version 450

layout (location = 0) in vec3 fragColor;
layout (location = 1) in vec2 fragTexCoord;

layout (binding = 1) uniform sampler2D texSampler;

// specify the out location (index of the framebuffer attachment) and out variable
layout (location = 0) out vec4 outColor;

void main(){
    //outColor = vec4(fragColor, 1.0); // rgba color, range [0, 1]
    //outColor = vec4(fragTexCoord, 0.0, 1.0);

    outColor = texture(texSampler, fragTexCoord);
}