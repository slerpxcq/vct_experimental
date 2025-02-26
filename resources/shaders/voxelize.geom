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
    flat int dominantAxis;
} gs_out;

uniform vec3 u_sceneAABB[2];

/* Voxelization 
 * 1. Select the dominant axis 
 * 2. Swizzle the components, making the dominant axis always facing z-axis
 * 3. In FS, swizzle back components and write to image
 */

int DominantAxis()
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

vec3 ToNDC(vec3 v)
{
    return vec3(((v - u_sceneAABB[0]) / (u_sceneAABB[1] - u_sceneAABB[0]) - vec3(0.5)) * 2);
}

void main()
{
    /* Select dominant axis */
    gs_out.dominantAxis = DominantAxis();

    for (int i = 0; i < 3; ++i) {
        vec3 ndcCoord = ToNDC(gl_in[i].gl_Position.xyz);
        gl_Position.xyz = (gs_out.dominantAxis == 0) ? ndcCoord.zyx :
                          (gs_out.dominantAxis == 1) ? ndcCoord.xzy :
                          ndcCoord;
        gl_Position.w = 1;
        EmitVertex();
    }
}