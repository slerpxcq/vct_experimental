# 2025/3/4
Shader auto reload when changed

# 2025/3/6
0. Analyze whole rendering process of VCTRenderer
1. Read the indirect lighting code
2. Implement voxel indirect lighting

# 2025/3/7
1. 

# 2025/3/12
- Voxel indirect lighting diffuse cone (No aniso filtering)

NOTE:
Whole render process
1. Voxelize ccene (VS + GS + FS)
2. Update radiance
- Inject voxel radiance
    - Calculate shadow map - Ignored: using ray marching
    - Calculate direct lighting "inject_lighting" (CS)
    - Calculate indirect (first bounce) lighting "inject_propagation" (CS)
- Generate mipmap (CS)
3. Deferred render scene to albedo, normal and emissive (VS + FS)
4. Do voxel cone tracing on screen quad (FS)