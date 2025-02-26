#version 460 core

out VS_OUT
{
    vec2 texCoord;
} vs_out;

void main()
{
    const vec4 vertices[4] = vec4[4](
        vec4(-1, -1, 0, 1),
        vec4(-1,  1, 0, 1),
        vec4( 1, -1, 0, 1),
        vec4( 1,  1, 0, 1));

    const uint indices[6] = uint[6](
        0, 2, 1,
        1, 2, 3);

    gl_Position = vertices[indices[gl_VertexID]];
    vs_out.texCoord = (gl_Position.xy + vec2(1)) * 0.5;
}