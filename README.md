ReSTIR - Vulkan
================
CIS 565: *GPU Programming and Architecture* Final Project

 - **Xuanyi Zhou** [[GitHub](https://github.com/lukedan)] [[LinkedIn](https://www.linkedin.com/in/xuanyi-zhou-661365192/)]
 - **Xuecheng Sun** [[GitHub](https://github.com/hehehaha12139)] [[LinkedIn](https://www.linkedin.com/in/hehehaha12138/)]
 - **Jiarui Yan** [[GitHub](https://github.com/WaikeiChan)] [[LinkedIn](https://www.linkedin.com/in/jiarui-yan-a06bb5197/)]

![Demo (Sponza)](media/title.png)

## Project Goal

This is the final project for CIS 565: GPU Programming. The goal of the project is to implement ‘Spatiotemporal Reservoir Resampling for Real-Time Ray Tracing with Dynamic Direct Lighting’ or ReSTIR by using Vulkan. It enables the usage of a large number of point and surface lights by combining various sampling techniques. This project uses the Vulkan API. Since the algorithm uses ray tracing for visibility testing, we explore using the Vulkan extension VK_KHR_ray_tracing for ray tracing. In addition, we also use Vulkan’s compute shader to implement a ray tracer, and compare the differences between them to evaluate the benefits of the Vulkan RT pipeline. Meanwhile, this project also supports various material types.

## How to Build and Run

 0. Make sure you have the required software installed:
    - Visual Studio (2019)
    - vcpkg (imgui[glfw-binding], imgui[vulkan-binding], glfw3, vulkan)
    - Nvidia driver (lastest)

 1. Clone this repository.

 2. Right Click in your ../ReSTIR-Vulkan/ folder and click Open with Visual Studio.

## Project Timeline
### Milestone 1 (Nov. 18)
 - GBuffer /Rasterization renderer.
 - Basic Compute Shader raytracing.
 - Basic Vulkan RT pipeline raytracing.
 - GLTF scene loader.
 - Review ReSTIR algorithm.

[MS1 Slides](media/milestone1_v3.pdf)
### Milestone 2 (Nov. 30)
 - Baised ReSTIR Algorithm.
 - Disney Principle BRDF Materials.

[MS2 Slides](media/milestone2_v2.pdf)


## Acknowledgments
 - [1] [Spatiotemporal Variance-Guided Filtering: Real-Time Reconstruction for Path-Traced Global Illumination](https://cs.dartmouth.edu/wjarosz/publications/bitterli20spatiotemporal.html)
 - [2] [Dear ImGui](https://github.com/ocornut/imgui)
 - [3] [VulkanMemoryAllocator](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git)
 - [4] [tinygltf](https://github.com/syoyo/tinygltf.git)
