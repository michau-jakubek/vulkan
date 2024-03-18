<h1>Vulkan Trivial Framework (VTF)</h1>

<h3>About:</h3>
<p align="justify">
Vulkan is excellent and awesome. Although it is terribly huge, I'm fascinated by it and I want to learn it every day, deeper and depper. I recognize there is a lot of similar 'frameworks' on the Internet but I wanted to have my own. This is actually VTF. The main idea when writing the code was to get the fastest result with as little code as possible. The project was written absolutely in C ++ in the C++17 standard with lots of language templates and features. As you study the code, you will find some interesting solutions, I think. For example, the VertexInput class tries to wrap everything up for creating an interface for shaders with automatic conversions from STL's vectors. In turn, the PipelineLayout class deals with the improvement of the creation of a wide range of descriptor sets and pipelines themselves. Other methods or routines generalize to a greater or lesser extent the Vulkan API.<br>I hope one will find the following sources useful not only for fun but also for getting to know Vulkan better.</p> 

<h3><a href="help/installation.html">Installation</a>, <a href=help/building.html>building</a> and <a href="help/launching.html">launching</a></h3>

<h3>Triangle test</h3>
In my opinion "Hello Triangle" must exist in every presentation, thus...<BR />
<img src="triangle.png" alt="triangle" /><BR />
This test might be run with <code>-t num_threads</code>parameter as well, where it means a number of GPU threads you may want to
 employ.<BR />In the mean time you can see how small amount of source code is needed to draw the triangle.
<h3>Fractals test</h3>
<p align="justify">
This test will take you to a land where you can admire the beauty of the Mandelbrot's fractal. While holding down the mouse button, find any place where you would like to dive and then, by scrolling, go deeper and deeper. The test automatically detects if your machine supports Float64. If so, the journey will be even more interesting. Additionally I've add <code>-a msesc</code> execution parameter that allows to run animated fractal refreshed avery msecs period, then you can use mice buttons or control keys to manually manipulate of zoom.
If -a parameter is present an application shows some interesting information, e.g. pseudo frame rate.
</p>
<img src="fractals.png" alt="fractals" />
<h3>Panorma viewer test</h3>
<p align="justify">
In turn, this test acts as a simple image viewer that can process several well-known formats, for example JPG. 
Additionally, the test allows you to view panoramic photos that are passed with the -p parameter, then you can move
around inside the image. It turns out that the reading of the image from the storage is a serious bottle-neck, so don't
worry if you will see a splash while the image is loading. Picture is loaded exactly once, next time if selected, it is
taken from a cache.
</p>
<h3>A few smaller tests that do not need description</h3>
<ul>
  <li><b>int_matrix</b> - test of internal matrices</li>
  <li><b>line_width</b> - verifies that lines has been drawn correctly (uses many renderpasses)</li>
  <li><b>toplogy</b> - allows interactively draw Vulkan's primitives depending on run parameters</li>
</ul>
<h3>Solutions you might be interested in</h3>
<ul>
  <li>main rendering loop callback</li>
  <li>VertexInput class that simplifies of building vertex input</li>
  <li>LayoutManager class that simplifies a work with descriptor sets</li>
  <li>template C++ function to create graphics pipeline</li>
  <li>template C++ function to manage barriers and barriers themself</li>
  <li>compile time conversion between C++ built-in types and VkFormat</li>
  <li>working with several renderpasses</li>
  <li>dynamic rendering</li>
</ul>
<h2>Enjoy!</h2>

