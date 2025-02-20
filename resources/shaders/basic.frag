#version 460

layout (location = 0) out vec4 f_color0;

layout (binding = 0) uniform sampler2D u_diffuseTex;
layout (binding = 1) uniform sampler2D u_specularTex;
layout (binding = 2) uniform sampler2D u_ambienTex;
layout (binding = 3) uniform sampler2D u_emissiveTex;
layout (binding = 4) uniform sampler2D u_heightTex;
layout (binding = 5) uniform sampler2D u_normalTex;
layout (binding = 6) uniform sampler2D u_shininessTex;
layout (binding = 7) uniform sampler2D u_opacityTex;

uniform bool u_hasMap[8];

in VS_OUT
{
    vec3 normal;
    vec2 texCoord;
} fs_in;

void main()
{
    if (u_hasMap[0]) {
        vec4 color = vec4(texture(u_diffuseTex, fs_in.texCoord));
        if (u_hasMap[7]) {
            color *= texture(u_opacityTex, fs_in.texCoord).r;
        }
        f_color0 = color;
    } else {
        f_color0 = vec4(fs_in.texCoord, 0, 1);
    }
}