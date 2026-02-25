# m1VulkanEngine

This is a personal project to learn Vulkan and moder C++.

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
*   Import and render `.gltf` / `.glb` meshes (triangle primitives).
*   Basic phong lighting.
*   Shadow mapping.
*   Compute shader to animate a particle system.

## Notes

This project is still under development as I continue studying Vulkan.



## glTF support

The project uses [fastgltf](https://github.com/spnda/fastgltf) for loading glTF files.
The repository expects fastgltf to be available under `third_party/fastgltf` (including its `deps/simdjson` folder).
