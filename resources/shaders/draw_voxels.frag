#version 460 core

layout (location = 0) out vec4 f_color;

in GS_OUT
{
    vec4 color;
} fs_in;

void main()
{
    //f_color = vec4(fs_in.color, 0, 0, 1);
    f_color = fs_in.color;
}