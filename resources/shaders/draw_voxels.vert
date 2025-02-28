#version 460 core

layout (rgba32f, binding = 0) uniform coherent readonly image3D u_voxelImage;
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

void main()
{
    ivec3 imageCoord = ivec3(gl_VertexID % u_voxelResolution,
                           (gl_VertexID / u_voxelResolution) % u_voxelResolution,
                            gl_VertexID / (u_voxelResolution * u_voxelResolution));

    vs_out.color = imageLoad(u_voxelImage, imageCoord);
    gl_Position = vec4(imageCoord, 1);
}
