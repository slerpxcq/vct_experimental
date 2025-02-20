@echo off

：： assimp
msbuild 3rdparty/assimp/build/Assimp.sln /p:Configuration=MinSizeRel
msbuild 3rdparty/assimp/build/Assimp.sln /p:Configuration=Debug