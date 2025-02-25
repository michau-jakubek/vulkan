## Installation
### Prerequisites
* Vulkan SDK
* GLFW library
* Git
* CMake
* Python
* C++ compilers

### Prepare the necessary items:
1. **Download and install Vulkan SDK**:
   - [https://vulkan.lunarg.com/](https://vulkan.lunarg.com/)
2. **Download and install GLFW library**:
   - [https://www.glfw.org/download](https://www.glfw.org/download)
3. **Download and install CMake build system**:
   - [https://cmake.org/download/](https://cmake.org/download)
4. **Install Git and Python**:
   - Use your preferred methods.
5. **Install compilers**:
   - On Linux: Preferably `g++` or `clang++`.
   - On Windows: Visual Studio.
6. **Clone the project repository**:
   - Run: `git clone https://github.com/michau-jakubek/vulkan.git vtf`
7. **Navigate to the newly created "vtf" folder of the project**:
   - Use the command: `cd vtf`
8. **Create a build folder inside "vtf"**:
   - Example: `mkdir build`
   - Navigate to the build folder: `cd build`

### Creating a project or a solution:
Creating a project is quite straightforward. To create the project, you need to run the `cmake`
tool with several parameters. Besides the built-in `cmake` parameters (e.g. `CMAKE_BUILD_TYPE`),
the tool needs to know where to find the project's dependencies: Vulkan library, Vulkan headers,
GLFW library, and its headers.

Set the following variables for `cmake`:
- **VULKAN_LIB_PATH**: Points to the Vulkan link library.
- **VULKAN_INCLUDE_DIR**: Points to the Vulkan include path.
- **GLFW3_LIB_PATH**: Points to the GLFW v3 link library.
- **GLFW3_INCLUDE_DIR**: Points to the GLFW v3 include path.

For example, your command to create the project would look like:

### On Linux:
```bash
cmake -DCMAKE_BUILD_TYPE=DEBUG \
      -DVULKAN_INCLUDE_DIR=/opt/vulkan/1.3.204.1/x86_64/include \
      -DVULKAN_LIB_PATH=/opt/vulkan/1.3.204.1/x86_64/lib/libvulkan.so \
      -DGLFW3_INCLUDE_DIR=/usr/include \
      -DGLFW3_LIB_PATH=/usr/lib/x86_64-linux-gnu/libglfw.so.3 ..
```
### On Windows:
```bash
cmake -G "Visual Studio 16 2019" ^
      -DVULKAN_INCLUDE_DIR=c:\\VulkanSDK\\1.3.216.0\\Include ^
      -DVULKAN_LIB_PATH=c:\\VulkanSDK\\1.3.216.0\\Lib\\vulkan-1.lib ^
      -DGLFW3_INCLUDE_DIR=c:\\VulkanDeps\\glfw-3.3.8.bin.WIN64\\include ^
      -DGLFW3_LIB_PATH=c:\\VulkanDeps\\glfw-3.3.8.bin.WIN64\\lib-vc2019\\glfw3.lib ..
```
Next, run the build command:
```bash
cmake --build . -j 23
```
Additionally, you can use the `-DVULKAN_CUSTOM_DRIVER=ON` flag to run the program with
a driver other than the default. The default driver name can be specified using
the `-DVULKAN_DRIVER` flag. On Windows, this will typically be `vulkan-1.dll`, while on Linux,
it should be the full path to the `libvulkan.so` library that you link against.

## Suggestions:
Please note that the `cmake` likes to hold values passed to the variables between
subsequent calls in its cache, especially if you call it with different variables. Thus it is good
idea to enforce it to configure always fresh build tree by using the _`--fresh`_ option.  
Another thing is that the Windows OS is case insensitive. In the contrast, the Linux OS is case
sensitive, so be carefully while you pass folder or subfolder names to mentioned variables.
