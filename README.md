# m1VulkanEngine

This is a personal project to learn Vulkan and moder C++.

The project is mainly based on the tutorial at [vulkan-tutorial.com](https://vulkan-tutorial.com/), but it was reworked to have a more structured, engine-like architecture, with the goal of improving code readability and maintainability and facilitates future developments.

Several C++ RAII wrapper classes are used to manage core Vulkan objects such as the device, swap chain, pipeline, and buffers.

The code is heavily commented to make Vulkan concepts easier to understand.

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

## Optional ImGui integration

The engine now exposes runtime toggles through an ImGui panel (MSAA, particles, and a shadow flag placeholder).

To enable ImGui at build time, set one of:

* `IMGUI_PATH` in `.env.cmake` pointing to an ImGui checkout.
* `third_party/imgui` in this repository (same layout as upstream ImGui).

If neither is present, the engine builds without ImGui support and UI rendering is disabled automatically.

## Notes

This project is still under development as I continue studying Vulkan.

