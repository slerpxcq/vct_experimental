#version 460 core

layout (points) in;
layout (triangle_strip, max_vertices = 24) out;

uniform vec3 u_sceneAABB[2]; // TODO: sceneAABB is not guaranteed to be cubic

uniform uint u_voxelResolution;
uniform mat4 u_view;
uniform mat4 u_proj;

in VS_OUT
{
    vec4 color;
} gs_in[1];

out GS_OUT
{
    vec4 color;
} gs_out;

void main()
{
    const vec3 vertices[8] = vec3[8](
        vec3(0, 0, 0),
        vec3(0, 0, 1),
        vec3(0, 1, 0),
        vec3(0, 1, 1),
        vec3(1, 0, 0),
        vec3(1, 0, 1),
        vec3(1, 1, 0),
        vec3(1, 1, 1));
    
    const int indices[24] = int[24](
		0, 2, 1, 3, // right
		6, 4, 7, 5, // left
		5, 4, 1, 0, // up
		6, 7, 2, 3, // down
		4, 6, 0, 2, // front
		1, 3, 5, 7); // back

    if (gs_in[0].color.a == 0) 
        return;

    vec3 voxelWorldPos = mix(u_sceneAABB[0], u_sceneAABB[1], gl_in[0].gl_Position.xyz / u_voxelResolution);
    vec3 voxelSize = (u_sceneAABB[1] - u_sceneAABB[0]) / u_voxelResolution;

    vec4 projectedVertices[8];
    for (int i = 0; i < 8; ++i) {
        projectedVertices[i] = u_proj * u_view * vec4(voxelWorldPos + voxelSize * vertices[i], 1);
    }

    for (int i = 0; i < 6; ++i) {
        for (int j = 0; j < 4; ++j) {
            gl_Position = projectedVertices[indices[4 * i + j]];
            // gs_out.color = gs_in[0].color;
            gs_out.color = vec4(gl_in[0].gl_Position.xyz / u_voxelResolution, 1);
            EmitVertex();
        }
        EndPrimitive();
    }
}