#!env bash

time "$1" -B build --fresh -DVULKAN_INCLUDE_DIR=/c/tools/vulkansc-sdk-1.0.15/sdk/include/ -DVULKAN_LIB_PATH=/c/tools/vulkansc-sdk-1.0.15/sdk/lib64/vulkansc-1.lib -DGLFW3_INCLUDE_DIR=/c/tools/glfw/out/include/ -DGLFW3_LIB_PATH=/c/tools/glfw/out/lib/glfw3.lib -DVULKAN_DRIVER=vulkansc-1.dll -DVULKAN_CUSTOM_DRIVER=ON -DVTF_SAFETY_CRITICAL=ON
