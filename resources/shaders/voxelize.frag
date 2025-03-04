#version 460 core
// May not supported on AMD cards
#extension GL_ARB_fragment_shader_interlock : require

layout (pixel_interlock_unordered) in;
layout (pixel_center_integer) in vec4 gl_FragCoord;

layout (rgba16f, binding = 0) uniform coherent image3D u_voxelColorImage;
layout (rgba16f, binding = 1) uniform coherent image3D u_voxelNormalImage;

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
    vec3 normal;
    vec2 texCoord;
    flat uint dominantAxis;
} fs_in;

// RGB stores color, A stores count
void ImageAverageColor(ivec3 imageCoord, vec3 color)
{
    vec4 oldColor = imageLoad(u_voxelColorImage, imageCoord);
    vec4 newColor;
    newColor.a = oldColor.a + 1.f;
    newColor.rgb = (oldColor.a * oldColor.rgb + color) / newColor.a;
    imageStore(u_voxelColorImage, imageCoord, newColor);
}

void ImageAverageNormal(ivec3 imageCoord, vec3 normal)
{
    vec4 oldNormal = imageLoad(u_voxelNormalImage, imageCoord);
    vec4 newNormal;
    newNormal.a = oldNormal.a + 1.f;
    newNormal.rgb = (oldNormal.a * oldNormal.rgb + normal) / newNormal.a;
    imageStore(u_voxelNormalImage, imageCoord, newNormal);
}

void main()
{
    ivec3 voxelCoord;
    voxelCoord.xy = ivec2(gl_FragCoord.xy);
    voxelCoord.z = int(gl_FragCoord.z * u_voxelResolution);
    voxelCoord = (fs_in.dominantAxis == 0) ? voxelCoord.zyx :
                 (fs_in.dominantAxis == 1) ? voxelCoord.xzy :
                 voxelCoord;

    beginInvocationInterlockARB();
    ImageAverageColor(voxelCoord, texture(u_diffuseTex, fs_in.texCoord).rgb);
    ImageAverageNormal(voxelCoord, fs_in.normal);
    endInvocationInterlockARB();
}