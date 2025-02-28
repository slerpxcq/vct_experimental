#version 460 core
// May not supported on AMD cards
#extension GL_ARB_fragment_shader_interlock : require

layout (pixel_interlock_unordered) in;
layout (pixel_center_integer) in vec4 gl_FragCoord;
layout (rgba32f, binding = 0) uniform coherent image3D u_voxelImage;

layout (binding = 0) uniform sampler2D u_diffuseTex;
layout (binding = 1) uniform sampler2D u_specularTex;
layout (binding = 2) uniform sampler2D u_ambienTex;
layout (binding = 3) uniform sampler2D u_emissiveTex;
layout (binding = 4) uniform sampler2D u_heightTex;
layout (binding = 5) uniform sampler2D u_normalTex;
layout (binding = 6) uniform sampler2D u_shininessTex;
layout (binding = 7) uniform sampler2D u_opacityTex;

uniform uint u_voxelResolution;

/* 
 * gl_FragCoord is in range of [0, 0, 0] - [VOXEL_RESOLUTION - 1, VOXEL_RESOLUTION - 1, 1]
 * imageCoord is in range of [0, 0, 0] - [VOXEL_RESOLUTION - 1, VOXEL_RESOLUTION - 1, VOXEL_RESOLUTION - 1]
 */

in GS_OUT
{
    flat uint dominantAxis;
    vec2 texCoord;
} fs_in;

// RGB stores color, A stores count
void ImageAverageRGB(ivec3 imageCoord, vec3 color)
{
    vec4 oldColor = imageLoad(u_voxelImage, imageCoord);
    vec4 newColor;
    newColor.a = oldColor.a + 1.f;
    newColor.rgb = (oldColor.a * oldColor.rgb + color) / newColor.a;
    imageStore(u_voxelImage, imageCoord, newColor);
}

void main()
{
    ivec3 imageCoord;
    imageCoord.xy = ivec2(gl_FragCoord.xy);
    imageCoord.z = int(gl_FragCoord.z * u_voxelResolution);
    imageCoord = (fs_in.dominantAxis == 0) ? imageCoord.zyx :
                 (fs_in.dominantAxis == 1) ? imageCoord.xzy :
                 imageCoord;

    beginInvocationInterlockARB();
    ImageAverageRGB(imageCoord, texture(u_diffuseTex, fs_in.texCoord).rgb);
    endInvocationInterlockARB();
}