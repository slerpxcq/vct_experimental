#version 460 core

#define DRAW_MODE_COLOR 0
#define DRAW_MODE_NORMAL 1

layout (rgba16f, binding = 0) uniform coherent readonly image3D u_voxelColorImage;
layout (rgba16f, binding = 1) uniform coherent readonly image3D u_voxelNormalImage;

uniform uint u_voxelResolution;
uniform uint u_drawMode;

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

    if (u_drawMode == DRAW_MODE_COLOR) {
        vs_out.color = imageLoad(u_voxelColorImage, imageCoord);
    } else {
        vec4 tmp = imageLoad(u_voxelNormalImage, imageCoord);
        vec3 normal = tmp.rgb;
        vs_out.color.rgb = 0.5f * (normalize(normal) + vec3(1));
        vs_out.color.a = tmp.a;
    }

    gl_Position = vec4(imageCoord, 1);
}
