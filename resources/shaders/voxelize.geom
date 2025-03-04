#version 460 core

layout (triangles) in;
layout (triangle_strip, max_vertices = 3) out;

in VS_OUT 
{
    vec3 normal;
    vec2 texCoord;
} gs_in[3];

out GS_OUT
{
    vec3 normal;
    vec2 texCoord;
    flat uint dominantAxis;
} gs_out;

uniform vec3 u_sceneAABB[2];
uniform uint u_axis;

/* Voxelization 
 * 1. Select the dominant axis 
 * 2. Swizzle the components, making the dominant axis always facing z-axis
 * 3. In FS, swizzle back components and write to image
 */

uint DominantAxis()
{
	vec3 p1 = gl_in[1].gl_Position.xyz - gl_in[0].gl_Position.xyz;
	vec3 p2 = gl_in[2].gl_Position.xyz - gl_in[0].gl_Position.xyz;
	vec3 faceNormal = cross(p1, p2);

	float nDX = abs(faceNormal.x);
	float nDY = abs(faceNormal.y);
	float nDZ = abs(faceNormal.z);

	return (nDX > nDY && nDX > nDZ) ? 0 :
	       (nDY > nDX && nDY > nDZ) ? 1 : 2;
}

vec3 WorldToNDC(vec3 v)
{
    return vec3(((v - u_sceneAABB[0]) / (u_sceneAABB[1] - u_sceneAABB[0]) - vec3(0.5)) * 2);
}

void main()
{
    uint dominantAxis = DominantAxis();

    // Discard triangle if not current axis
    if (dominantAxis != u_axis) 
        return;

    gs_out.dominantAxis = dominantAxis;

    for (int i = 0; i < 3; ++i) {
        vec3 ndcCoord = WorldToNDC(gl_in[i].gl_Position.xyz);
        gl_Position.xyz = (dominantAxis == 0) ? ndcCoord.zyx :
                          (dominantAxis == 1) ? ndcCoord.xzy :
                          ndcCoord;
        gl_Position.w = 1;
        gs_out.texCoord = gs_in[i].texCoord;
        gs_out.normal = gs_in[i].normal;
        EmitVertex();
    }
}