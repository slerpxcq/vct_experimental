#version 460 core

layout (location = 0) out vec4 f_color0;

in VS_OUT 
{
    vec4 color;
} fs_in;

void main()
{
    f_color0 = fs_in.color;
}