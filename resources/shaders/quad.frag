#version 460 core

layout (location = 0) out vec4 f_color0;

in VS_OUT 
{
    vec2 texCoord;
} fs_in;

void main()
{
    f_color0 = vec4(fs_in.texCoord, 0, 1);
}