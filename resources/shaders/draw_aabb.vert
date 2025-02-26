#version 460 core

uniform vec3 u_sceneAABB[2];
uniform mat4 u_proj;
uniform mat4 u_view;

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

    const uint indices[24] = uint[24](
        0, 1,
        0, 2,
        1, 3,
        2, 3,
        0, 4,
        1, 5,
        2, 6,
        3, 7,
        4, 5,
        4, 6,
        5, 7,
        6, 7);

    vec3 pos = mix(u_sceneAABB[0], u_sceneAABB[1], 
                   vertices[indices[gl_VertexID]]);

    gl_Position = u_proj * u_view * vec4(pos, 1);
}