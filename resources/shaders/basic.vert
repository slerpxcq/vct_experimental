#version 460

layout (location = 0) in vec3 a_position;
layout (location = 1) in vec3 a_normal;
layout (location = 2) in vec2 a_texCoord;

uniform mat4 u_view;
uniform mat4 u_proj;

out VS_OUT
{
    vec3 normal;
    vec2 texCoord;
} vs_out;


void main()
{
    vs_out.normal = a_normal;
    vs_out.texCoord = a_texCoord;
    gl_Position = u_proj * u_view * vec4(a_position, 1);
}
