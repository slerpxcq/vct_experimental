# Voxelization
1. exist/non-exist

# Draw voxels
- For each texel, use geometry shader to generate a cube

# Synchronization
Because of component swizzling, different fragment could write to the same voxel, causing GL_ARB_fragment_shader_interlock to be useless.
Solution: 
- spinlock
- one pass for each axis

# Voxel direct light
One directional light only for now