#version 460 core

layout (r32ui, binding = 0) uniform coherent uimage3D u_voxelImage;
layout (pixel_center_integer) in vec4 gl_FragCoord;

uniform uint u_voxelResolution;

/* 
 * gl_FragCoord is in range of [0, 0, 0] - [VOXEL_RESOLUTION - 1, VOXEL_RESOLUTION - 1, 1]
 * imageCoord is in range of [0, 0, 0] - [VOXEL_RESOLUTION - 1, VOXEL_RESOLUTION - 1, VOXEL_RESOLUTION - 1]
 */

in GS_OUT
{
    flat int dominantAxis;
} fs_in;

uint PackColor(vec4 color)
{
    return ((uint(color.r * 255) & 0xff) << 24) | 
           ((uint(color.g * 255) & 0xff) << 16) | 
           ((uint(color.b * 255) & 0xff) << 8) | 
           ((uint(color.a * 255) & 0xff));
}

void main()
{
    ivec3 imageCoord;
    imageCoord.xy = ivec2(gl_FragCoord.xy);
    imageCoord.z = int(gl_FragCoord.z * u_voxelResolution);
    imageCoord = (fs_in.dominantAxis == 0) ? imageCoord.zyx :
                 (fs_in.dominantAxis == 1) ? imageCoord.xzy :
                 imageCoord;

    // vec3 color = vec3(vec2(imageCoord.xy) / u_voxelResolution, 0);
    vec3 color = vec3(1, 0, 0);

    imageAtomicExchange(u_voxelImage, imageCoord, PackColor(vec4(color, 1)));
}