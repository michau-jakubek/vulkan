#!/usr/bin/bash

cmake -B ./build/ -DCMAKE_CXX_COMPILER=clang++-18 -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DGLFW3_LIB_PATH=/home/gek/projects/glfw/out/lib/libglfw3.a -DGLFW3_INCLUDE_DIR=/home/gek/projects/glfw/out/include -DVULKAN_SDK=/home/gek/projects/vulkan-sdk-1.4.350.1/x86_64/ -DOFFLINE_SHADER_COMPILER=ON -DVTF_AS_DLL=OFF -DVTF_LIBS_DIR=/home/gek/projects/mojsze/vtf-libs-repo/

# VK_LAYER_PATH=/home/gek/projects/vulkan-sdk-1.4.350.1/x86_64/share/vulkan/explicit_layer.d LD_LIBRARY_PATH=/home/gek/projects/vulkan-sdk-1.4.350.1/x86_64/lib ./build/app/vtf -l VK_LAYER_KHRONOS_validation -d 1 -compiler -1 -tmp ./run descriptor_heap
# VK_LAYER_PATH=/home/gek/projects/vulkan-sdk-1.4.350.1/x86_64/share/vulkan/explicit_layer.d ./build/app/vtf-launcher -l VK_LAYER_KHRONOS_validation -tmp ./run triangle
