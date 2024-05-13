Vulkan Trivial Framework (VTF)
========
The main idea when writing this code was to get the fastest result with as little code as possible. The project was written in C++ language with the fetures available since the C++17 standard and my favorite one in C++, which is a templates. As you study the code, you will find some interesting solutions, I think. For example, the _VertexInput_ class tries to wrap everything up for creating an interface for shaders with automatic conversions from STL's vectors. In turn, the _PipelineManager_ class deals with the improvement of the creation of a wide range of descriptor sets, whereas _createGraphicsPipeline_ template function simplyfies creation of pipeline via recognizing subsequent states by type. Other methods or routines generalize to a greater or lesser extent the Vulkan API.  
I hope one will find the following sources useful not only for fun but also for getting to know Vulkan better.

Supported OSs and Requirements and Licensing
---------
This stuff strongly uses dynamically linked [Vulkan SDK](https://vulkan.lunarg.com/sdk/home) which has to be installed separately. Along with Vulkan SDK some of their binaries must be visble to the final VTF executable e.g. _glslangValidator_ or _spirvas_. Next to the VKSDK  the sources of this repo uses statically linked [GLFW](https://www.glfw.org/) library for a rendering - which also has to be installed separately.  
I wrote this code mainly with the idea that it would run under Linux OS, but Both of those installations are available for Linux and Windows operating systems so you can successfully run it under Windows as well if you like. Other systems, e.g. MacOS or Android are not currently supported, although who knows.  
Together with aforementioned ones some third-party libraries are used, these are: _stb_image.h_ from [STB repository](https://github.com/nothings/stb) and _stl_reader.h_ from [STL_READER repository](https://github.com/sreiter/stl_reader). Both of them are automatically cloned from their Github repositories during the building process by the _fetch_externals.py_ script. Of course you can be up to date with those repos by calling the script from scratch.
- [Vulkan SDK license](https://vulkan.lunarg.com/license/)
- [GLFW license](https://www.glfw.org/license)
- [STB license](https://github.com/nothings/stb/blob/master/LICENSE)
- [STL_READER license](https://github.com/sreiter/stl_reader/blob/master/LICENSE)

[Installation](help/installation.html), [building](help/building.html) and [launching](help/launching.html)
------------------------

Shaders
-------
The shaders are built automatically at the first time when you are running particular test, although, there are the tests that force to build the shaders every time they are run. Shaders binaries are stored in the temprary system folder or in the location you can point as the parameter to the VTF executable. Basically the binaries are built every time their code changes. Then something kind of the checksum is calculated and if it does not match to the previous one then the shaders are rebuild.

Triangle test
-------------

In my opinion "Hello Triangle" must exist in every presentation regardless which technology it was written, so ...

![](triangle.png)

Beside the test is really trivial you can run it on more than one thread, of course if your GPU supports multithreading.

Fractals test
-------------

![](fractals.png)

This test will take you to a land where you can admire the beauty of the Mandelbrot's fractal. While holding down the mouse button,
find any place where you would like to dive and then, by scrolling, go deeper and deeper. The test automatically detects if your machine
supports Float64. If so, the journey will be even more interesting. Additionally you can run the test in animation mode then using a mice or a touchpad along with keyboard keys will cause the fractal will be zoomed in or out every millisecond you passed as a parameter.

Panorma viewer test
-------------------

In turn, this test acts as a simple image viewer that can process several well-known formats, for example JPG, PNG, etc (thanks to stb_image). Additionally, the test allows to render panoramic photos that are passed with the `-p` parameter, then you can move
around inside the image. It turns out that the reading of the image from the storage is a serious bottle-neck, so don't
worry if you will see a splash while the image is loading. Pictures are loaded exactly once, next time if selected, they are
taken from a cache.

# Enjoy!