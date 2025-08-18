#version 450 // Specifies the GLSL version

vec2 positions[3] = vec2[] (
    vec2(-0.5, 0.5),
    vec2(0.5, 0.5),
    vec2(0.0, -0.5)
);

void main() {
    // gl_Position is a built-in output variable that stores the final vertex position in the vertex shader
    // Sets the final vertex position in clip space (range [-w, +w] for x, y, z. GPU uses them for clipping against the view frustum before perspective division to NDC.)
    // The x,y,z values will be converted in Normalized Device Coordinates (NDC) (range [-1, 1]) by dividing them by w
    gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);  
}