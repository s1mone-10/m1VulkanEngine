#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>

#include <iostream>

#include "Application.hpp"
#include "log/Log.hpp"

int main()
{
    va::Log::Get().Info("Application starting");
    va::App app;

    try
    {
        app.run();
    }
    catch (const std::exception &e)
    {
        va::Log::Get().Error(e.what());
        return EXIT_FAILURE;
    }

    va::Log::Get().Info("Application finished successfully");
    return EXIT_SUCCESS;
}