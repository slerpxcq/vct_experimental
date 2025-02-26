#version 460 core

layout (location = 0) in vec3 a_position;
layout (location = 1) in vec3 a_normal;
layout (location = 2) in vec2 a_texCoord;

out VS_OUT 
{
    vec3 normal;
    vec2 texCoord;
} vs_out;

// Pass through information to geometry shader
void main()
{
    vs_out.normal = a_normal;
    vs_out.texCoord = a_texCoord;
    gl_Position = vec4(a_position, 1);
}