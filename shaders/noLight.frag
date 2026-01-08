#version 450

// Input
layout (location = 0) in vec3 fragColor;

// Output. Specify the out location (index of the framebuffer attachment) and out variable
layout (location = 0) out vec4 outColor;

void main(){
    outColor = vec4(fragColor, 1.0); // rgba color, range [0, 1]
}