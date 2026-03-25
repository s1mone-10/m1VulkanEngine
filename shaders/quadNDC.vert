#version 450

layout (location = 0) out vec2 texCoord;

void main()
{
    // Fullscreen triangle (so large it completely contains the [-1, 1] screen space
    float x = -1.0 + float((gl_VertexIndex & 1) << 2);
    float y = -1.0 + float((gl_VertexIndex & 2) << 1);
    texCoord = vec2((x + 1.0) * 0.5, 1.0 -((y + 1.0) * 0.5));
    gl_Position = vec4(x, y, 0.0, 1.0);
}
