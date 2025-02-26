#version 460 core

#define AXES_LEN 1000

out VS_OUT 
{
    vec4 color;
} vs_out;

uniform mat4 u_proj;
uniform mat4 u_view;

void main()
{
    const vec4 vertices[6] = vec4[6](
        vec4(0, 0, 0, 1),
        vec4(AXES_LEN, 0, 0, 1),
        vec4(0, 0, 0, 1),
        vec4(0, AXES_LEN, 0, 1),
        vec4(0, 0, 0, 1),
        vec4(0, 0, AXES_LEN, 1));

    uint colorIndex = gl_VertexID / 2;

    gl_Position = u_proj * u_view * vertices[gl_VertexID];

    vs_out.color = (colorIndex == 0) ? vec4(1, 0, 0, 1) :
                   (colorIndex == 1) ? vec4(0, 1, 0, 1) :
                   vec4(0, 0, 1, 1);
}