# Voxelization
1. exist/non-exist

# Draw voxels
- For each texel, use geometry shader to generate a cube

# Synchronization
Because component swizzling, the same fragment is not corresponded to the same voxel, causing GL_ARB_fragment_shader_interlock to be useless.