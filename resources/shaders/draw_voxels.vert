#version 460 core

layout (r32ui, binding = 0) uniform coherent readonly uimage3D u_voxelImage;
uniform uint u_voxelResolution;

/* Draw voxels
 * 1. Generate lattice from gl_VertexID
 * 2. Sample the image color and pass to geometry shader 
 * 3. In GS, generate a cube for each vertex
 */

out VS_OUT
{
    vec4 color;
} vs_out;

vec4 UnpackColor(uint uColor)
{
    return vec4(((uColor & 0xff000000) >> 24) / 255,
               ((uColor & 0xff0000) >> 16) / 255,
               ((uColor & 0xff00) >> 8) / 255,
               (uColor & 0xff) / 255);
}

void main()
{
    ivec3 imageCoord = ivec3(gl_VertexID % u_voxelResolution,
                           (gl_VertexID / u_voxelResolution) % u_voxelResolution,
                            gl_VertexID / (u_voxelResolution * u_voxelResolution));

    vs_out.color = UnpackColor(imageLoad(u_voxelImage, imageCoord).r);
    gl_Position = vec4(imageCoord, 1);
}
