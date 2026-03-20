#version 450

layout (location = 0) out vec3 dir;

layout(push_constant) uniform Push {
    mat4 projection;
    mat4 view;
} push;

const vec3 vertices[8] = vec3[8](
    vec3(-1.0,-1.0, 1.0),		// 0
    vec3( 1.0,-1.0, 1.0),		// 1
    vec3( 1.0, 1.0, 1.0),		// 2
    vec3(-1.0, 1.0, 1.0),		// 3

    vec3(-1.0,-1.0,-1.0),		// 4
    vec3( 1.0,-1.0,-1.0),		// 5
    vec3( 1.0, 1.0,-1.0),		// 6
    vec3(-1.0, 1.0,-1.0)		// 7
);

const int indices[36] = int[36](
    1, 0, 2, 3, 2, 0,	// front
    5, 1, 6, 2, 6, 1,	// right
    6, 7, 5, 4, 5, 7,	// back
    0, 4, 3, 7, 3, 4,	// left
    5, 4, 1, 0, 1, 4,	// bottom
    2, 3, 6, 7, 6, 3	// top
);

void main()
{
    int idx = indices[gl_VertexIndex];
    dir = vertices[idx];
    vec4 pos = push.projection * push.view * vec4(dir, 1.0);
    gl_Position = pos.xyww; // force Z component to max depth value 1.0
}
