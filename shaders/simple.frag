#version 450

// specify the out location (index of the framebuffer attachment) and out variable
layout (location = 0) out vec4 outColor;

void main(){
    outColor = vec4(1.0, 1.0, 0.0, 1.0); // rgba color, range [0, 1]
}