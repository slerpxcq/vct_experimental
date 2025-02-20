@echo off

:: -------- assimp --------
pushd .
cd "3rdparty/assimp"
cmake -S . -B build -DBUILD_SHARED_LIBS=OFF
popd

"3rdparty/premake5/premake5" vs2022