#version 460 core

#define DRAW_MODE_ALBEDO 0
#define DRAW_MODE_NORMAL 1
#define DRAW_MODE_EMISSIVE 2
#define DRAW_MODE_RADIANCE 3
#define DRAW_MODE_OCCLUSION 4

layout (rgba16f, binding = 0) uniform coherent readonly image3D u_voxelColorImage;
layout (rgba16f, binding = 1) uniform coherent readonly image3D u_voxelNormalImage;
layout (rgba16f, binding = 2) uniform coherent readonly image3D u_voxelEmissiveImage;
layout (rgba16f, binding = 3) uniform coherent readonly image3D u_voxelRadianceImage;

layout (binding = 4) uniform sampler3D u_voxelRadianceTexture;

uniform uint u_voxelResolution;
uniform uint u_drawMode;
uniform uint u_mipLevel;

#define EPSILON 1e-4f

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

    if (u_drawMode == DRAW_MODE_ALBEDO) {
        vs_out.color = imageLoad(u_voxelColorImage, imageCoord);
    } else if (u_drawMode == DRAW_MODE_NORMAL) {
        vec4 tmp = imageLoad(u_voxelNormalImage, imageCoord);
        vec3 normal = tmp.rgb;
        vs_out.color.rgb = 0.5f * (normalize(normal) + vec3(1));
        vs_out.color.a = tmp.a;
    } else if (u_drawMode == DRAW_MODE_EMISSIVE) {
        vs_out.color = imageLoad(u_voxelEmissiveImage, imageCoord);
    } else if (u_drawMode == DRAW_MODE_RADIANCE) { 
        vec3 texCoord = vec3(imageCoord) / u_voxelResolution;
        // vs_out.color = texture(u_voxelRadianceTexture, texCoord);
        // vs_out.color = textureLod(u_voxelRadianceTexture, texCoord, 2.f);
        vs_out.color = texelFetch(u_voxelRadianceTexture, imageCoord / int(pow(2, u_mipLevel)), int(u_mipLevel));
    } else { // DRAW_MODE_OCCLUSION
        vs_out.color = imageLoad(u_voxelRadianceImage, imageCoord);
    }

    gl_Position = vec4(imageCoord, 1);
}
