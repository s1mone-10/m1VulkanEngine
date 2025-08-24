# Vulkan App

A simple C++ application that demonstrates how to render a basic triangle using the Vulkan API.

## Getting Started

This guide will get you a copy of the project up and running on your local machine for development and testing purposes.

### Prerequisites

You will need to have the following software installed on your system:

*   [CMake](https://cmake.org/download/) (version 3.11 or higher)
*   [Vulkan SDK](https://vulkan.lunarg.com/sdk/home)
*   [GLFW](https://www.glfw.org/download.html)
*   [GLM](https://glm.g-truc.net/0.9.9/index.html)
*   A C++ compiler that supports C++23 (e.g., GCC, Clang, MSVC)

### Building

This project uses CMake to generate build files for your specific platform.

1.  **Clone the repository:**
    ```sh
    git clone https://github.com/your-username/VulkanApp.git
    cd VulkanApp
    ```

2.  **Configure Dependencies (Optional):**

    This project can find dependencies like Vulkan, GLFW, and GLM if they are installed in standard system locations. However, if you have them in custom directories, you can tell CMake where to find them by creating a `.env.cmake` file in the root of the project.

    Copy either `envWindowsExample.cmake` or `envUnixExample.cmake` to a new file named `.env.cmake` and modify the paths to match your system.

    *Example `.env.cmake` on Windows:*
    ```cmake
    set(GLFW_PATH C:/Libraries/glfw-3.4.bin.WIN64)
    set(GLM_PATH C:/Libraries/glm)
    set(VULKAN_SDK_PATH C:/VulkanSDK/1.3.268.0)

    # Set MINGW_PATH if you are using the MinGW compiler
    # set(MINGW_PATH "C:/Program Files/mingw64")
    ```

    *Example `.env.cmake` on Linux/macOS:*
    ```cmake
    # CMake can usually find these automatically on Unix-like systems
    # if installed via a package manager (e.g., apt, brew, pacman).
    # Only set these if they are in a non-standard location.
    # set(VULKAN_SDK_PATH /path/to/vulkan/sdk)
    # set(GLFW_PATH /path/to/glfw)
    # set(GLM_PATH /path/to/glm)
    ```

3.  **Generate Build Files:**

    Create a `build` directory and run CMake from within it.

    ```sh
    mkdir build
    cd build
    cmake ..
    ```
    *   **On Windows (Visual Studio):** CMake will generate a Visual Studio solution (`.sln`) file in the `build` directory.
    *   **On Windows (MinGW):** If you have MinGW configured in your `.env.cmake`, you can generate Makefiles by running: `cmake .. -G "MinGW Makefiles"`
    *   **On Linux/macOS:** CMake will generate Makefiles.

4.  **Compile the Project:**

    *   **Visual Studio:** Open the `.sln` file and build the `VulkanApp` project.
    *   **MinGW/Linux/macOS:** Run the `make` command from the `build` directory.
        ```sh
        make
        ```

## Dependencies

This project relies on a few key libraries to interface with the graphics API and windowing system.

*   **Vulkan SDK**: The core graphics API used for rendering. The SDK provides the headers, libraries, and validation layers needed to build a Vulkan application.
*   **GLFW**: A multi-platform library for creating windows, handling input, and managing OpenGL and Vulkan contexts. It is used here to create the window that the Vulkan triangle is rendered into.
*   **GLM (OpenGL Mathematics)**: A header-only C++ mathematics library for graphics software based on the GLSL specification. It is used for vector and matrix operations, though this simple example uses it minimally.

## Project Structure

The project is organized into the following main directories:

```
.
├── .env.cmake          # Optional file for custom dependency paths
├── shaders/
│   ├── simple.vert     # Vertex shader
│   └── simple.frag     # Fragment shader
├── src/
│   ├── main.cpp        # Application entry point
│   ├── Window.hpp      # Window class header
│   ├── Window.cpp      # Window class implementation
│   ├── VulkanApp.hpp   # Main Vulkan application class header
│   └── VulkanApp.cpp   # Main Vulkan application class implementation
├── CMakeLists.txt      # Main CMake build script
└── README.md           # This file
```

*   `src/`: Contains all the C++ source code.
    *   `VulkanApp`: A class that encapsulates all the Vulkan setup and rendering logic.
    *   `Window`: A class that manages the GLFW window.
*   `shaders/`: Contains the GLSL shader source code. These are compiled into SPIR-V by CMake and placed in the `shaders/compiled` directory (which is created during the build).
*   `CMakeLists.txt`: The heart of the build system. It defines the project, finds dependencies, and sets up the build targets.
*   `.env.cmake`: An optional file you can create to provide custom paths for your dependencies if they are not in standard locations.

## Running the Application

After a successful build, the executable will be located in the `build` directory.

*   **On Windows:** The executable will be `build/Debug/VulkanApp.exe` (or a similar path depending on your build configuration).
*   **On Linux/macOS:** The executable will be `build/VulkanApp`.

You can run it from the command line:

```sh
# On Linux/macOS
./build/VulkanApp

# On Windows (Command Prompt)
.\build\Debug\VulkanApp.exe
```

## Screenshot

Here is a screenshot of the application running on Windows:

<img width="402" height="323" alt="image" src="https://github.com/user-attachments/assets/adea8eb1-fb12-44d1-adf2-02fee26c3988" />
