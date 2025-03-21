#version 460 core

layout (location = 0) out vec4 f_color;

in GS_OUT
{
    vec4 color;
} fs_in;

void main()
{
    f_color = fs_in.color;
}