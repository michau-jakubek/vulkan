#!env bash

time "$1" -G "Visual Studio 18 2026" -B build --fresh -DVULKAN_SDK=/c/tools/VulkanSDK/1.4.321.1/ -DGLFW3_INCLUDE_DIR=/c/tools/glfw/out/include/ -DGLFW3_LIB_PATH=/c/tools/glfw/out/lib/glfw3.lib -Dglslang_DIR=/c/projects/others/glslang/out/lib/cmake/glslang/ -DSPIRV-Tools-opt_DIR=/c/projects/others/glslang/out/SPIRV-Tools-opt/cmake/ -DSPIRV-Tools_DIR=/c/projects/others/glslang/out/SPIRV-Tools/cmake/ -DOFFLINE_SHADER_COMPILER=ON "$2" "$3" "$4"

time "$1" --build ./build -j $(nproc)

echo "Done..."

./build/app/Debug/vtf -l VK_LAYER_KHRONOS_validation -tmp /r/Temp -compiler -1 -spvdisassm triangle

#export CMAKE="$(cygpath 'C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe')"
