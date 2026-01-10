# m1VulkanEngine

This is a personal project to learn Vulkan and C++.

It is mainly based on the tutorial at [vulkan-tutorial.com](https://vulkan-tutorial.com/), but I tried to create a structured engine to facilitate future developments.
I implemented several C++ wrapper classes to manage the main Vulkan objects like the device, swap chain, pipeline, and buffers.

## Project Structure

```
.
├── resources/          # 3D models and textures
├── shaders/
├── src/
│   ├── geometry/       # Geometric definitions such as mesh and vertex
│   ├── graphics/       # Core rendering engine
│   │   ├── Engine.hpp  # The main class that orchestrates the rendering process.
│   │   ├── Device.hpp
│   │   ├── SwapChain.hpp
│   │   ├── Pipeline.hpp
│   │   ├── ...
│   │   └── ...
│   ├── Window.hpp      # Wrapper class for the GLFW window
│   └── main.cpp
├── CMakeLists.txt
└── README.md

```

## Current Features

*   Load and render textured `.obj` models.
*   Basic phong lighting (no shadows).
*   Compute shader to animate a particle system.

## Future Plans

This project is still under development. My next steps could be:

*   Improve lighting and add shadows.
*   Improve rendering of multiple objects and using instancingg for identical objects.
*   Multithreading.
*   Continue studying and implementing more advanced Vulkan features.
