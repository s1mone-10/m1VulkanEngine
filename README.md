# m1VulkanEngine

This is a personal project to learn Vulkan and C++. It is mainly based on the awesome tutorial at [vulkan-tutorial.com](https://vulkan-tutorial.com/).

The main goal is to create a structured engine with a class-based architecture.

_Disclaimer: I used AI tools intensively to help me in this project. So there might be some errors or suboptimal solutions._

## Current Features

*   Load and render textured `.obj` models.
*   Basic lighting (no shadows).
*   Compute pipeline implementation.
*   The project structure follows the progress of the vulkan-tutorial.com tutorial.

## Future Plans

This project is still under development. My next steps are:

*   Implement multithreading for rendering.
*   Explore and implement instancing.
*   Continue studying and implementing more advanced Vulkan features.

## Project Structure

```
.
├── resources/          # 3D models and textures
├── shaders/
│   ├── simple_shader.frag
│   └── simple_shader.vert
├── src/
│   ├── graphics/       # Core rendering engine
│   │   ├── Engine.hpp
│   │   ├── Device.hpp
│   │   ├── SwapChain.hpp
│   │   └── Pipeline.hpp
│   ├── Window.hpp
│   └── main.cpp
├── CMakeLists.txt      # Main CMake build script
└── README.md           # This file
```

*   `src/`: Contains all the C++ source code.
    *   `graphics/`: Encapsulates the core Vulkan rendering engine.
        *   `Engine`: The main class that orchestrates the rendering process.
        *   `Device`: Manages the Vulkan logical and physical devices.
        *   `SwapChain`: Manages the swap chain for image presentation.
        *   `Pipeline`: Manages the graphics pipeline configuration.
    *   `Window`: A class that manages the GLFW window.
    *   `main.cpp`: The entry point of the application.
*   `shaders/`: Contains the GLSL shader source code. These are compiled into SPIR-V by CMake.
*   `resources/`: Contains 3D models and textures.
*   `CMakeLists.txt`: The main build script for the project.
