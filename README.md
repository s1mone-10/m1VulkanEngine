# m1VulkanEngine

This is a personal project to learn Vulkan and modern C++.

## Project Structure

```
.
├── resources/          # 3D models and textures
├── shaders/
├── src/
│   ├── geometry/       # Geometric definitions such as mesh and vertex
│   ├── graphics/       # Core rendering engine
│   ├── Window.hpp      # Wrapper class for the GLFW window
│   └── main.cpp
├── third_party/        # Local third-party sources (e.g. imgui)
└── CMakeLists.txt
```

## Current Features

- Load and render textured `.obj` models.
- Import and render `.gltf` / `.glb` meshes (triangle primitives).
- Basic Phong lighting.
- Shadow mapping.
- Compute shader to animate a particle system.

## Build

The project now manages dependencies through CMake (`FetchContent`) and no longer
requires configuring `GLFW_PATH`, `GLM_PATH`, `TINYOBJ_PATH`, etc. in `.env.cmake`.

Dependencies resolved automatically:
- glfw
- glm
- tinyobjloader
- VulkanMemoryAllocator
- stb
- fastgltf
- simdjson

System dependency required:
- Vulkan SDK / loader (discoverable via `find_package(Vulkan REQUIRED)`).

Build commands:

```bash
cmake -S . -B build
cmake --build build -j
```

## glTF support

The project uses [fastgltf](https://github.com/spnda/fastgltf) for loading glTF files.
`simdjson` is fetched automatically as part of the CMake configure step.
