ReSTIR - Vulkan
================
CIS 565: *GPU Programming and Architecture* Final Project

 - **Xuanyi Zhou** [[GitHub](https://github.com/lukedan)] [[LinkedIn](https://www.linkedin.com/in/xuanyi-zhou-661365192/)]
 - **Xuecheng Sun** [[GitHub](https://github.com/hehehaha12139)] [[LinkedIn](https://www.linkedin.com/in/hehehaha12138/)]
 - **Jiarui Yan** [[GitHub](https://github.com/WaikeiChan)] [[LinkedIn](https://www.linkedin.com/in/jiarui-yan-a06bb5197/)]

![Demo (Sponza)](media/title.png)

## Project Goal

This is the final project for CIS 565: GPU Programming. The goal of the project is to implement ‘Spatiotemporal Reservoir Resampling for Real-Time Ray Tracing with Dynamic Direct Lighting’ or ReSTIR using Vulkan. It enables the usage of a large number of point and surface lights and the rendering of their shadows by combining various sampling techniques. Since the algorithm uses ray tracing for visibility testing, we explore using the Vulkan extension `VK_KHR_ray_tracing` for ray tracing. In addition, we also use Vulkan’s compute shader to implement a ray tracer, and compare the performance differences between the two methods.

## Building the Project

 0. Prerequisites on Windows:
    - Visual Studio 2019
    - The following packages:
      - GLFW3
      - Vulkan SDK
      - Dear ImGUI with glfw and Vulkan bindings

      These packages can be installed easily using vcpkg.
    - Nvidia Vulkan driver that supports the `VK_KHR_ray_tracing` extension. See https://developer.nvidia.com/vulkan-driver, section 'Khronos Vulkan Ray Tracing'.

    The code was written with portability in mind, but it has not been tested on other platforms.

 1. Clone this repository with the `--recurse-submodules` flag.

 2. Build the project using the standard CMake building process. The executable expects the compiled shaders to be located in `shaders/`, and while this is usually automatically guaranteed by the building process, in some configurations they may need to be manually copied.

## Project Timeline
### Milestone 1 (Nov. 18)
 - GBuffer generation.
 - Compute shader raytracing.
 - Vulkan RT pipeline raytracing.
 - GLTF scene loader.
 - Reading the paper and understanding the algorithm.

[MS1 Slides](media/milestone1_v3.pdf)

### Milestone 2 (Nov. 30)
 - Baised ReSTIR Algorithm.
 - Disney Principle BRDF Materials.

[MS2 Slides](media/milestone2_v2.pdf)

## References and Acknowledgments
 - [1] [Spatiotemporal Variance-Guided Filtering: Real-Time Reconstruction for Path-Traced Global Illumination](https://cs.dartmouth.edu/wjarosz/publications/bitterli20spatiotemporal.html)
 - [2] [Dear ImGui](https://github.com/ocornut/imgui)
 - [3] [VulkanMemoryAllocator](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git)
 - [4] [tinygltf](https://github.com/syoyo/tinygltf.git)
