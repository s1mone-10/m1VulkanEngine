#version 450

layout (location = 0) out vec3 direction;

layout(push_constant) uniform Push {
    mat4 projView;
    float _;
} push;

const vec3 vertices[8] = vec3[8](
    vec3(-1, -1,  1),   // 0
    vec3( 1, -1,  1),   // 1
    vec3( 1,  1,  1),   // 2
    vec3(-1,  1,  1),   // 3

    vec3(-1, -1, -1),   // 4
    vec3( 1, -1, -1),   // 5
    vec3( 1,  1, -1),   // 6
    vec3(-1,  1, -1)    // 7
);

const int indices[36] = int[36](
    0, 1, 2, 2, 3, 0, // front
    1, 5, 6, 6, 2, 1, // right
    7, 6, 5, 5, 4, 7, // back
    4, 0, 3, 3, 7, 4, // left
    4, 5, 1, 1, 0, 4, // bottom
    3, 2, 6, 6, 7, 3  // top
);

void main()
{
    int idx = indices[gl_VertexIndex];
    direction = vertices[idx];
    gl_Position =  push.projView * vec4(direction, 1.0);
}
