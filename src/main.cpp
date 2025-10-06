#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <iostream>

#include "log/Log.hpp"
#include "graphics/Engine.hpp"

int main()
{
    m1::Log::Get().Info("Application starting");
    m1::Engine app;

    try
    {
        app.run();
    }
    catch (const std::exception &e)
    {
        m1::Log::Get().Error(e.what());
        return EXIT_FAILURE;
    }

    m1::Log::Get().Info("Application finished successfully");
    return EXIT_SUCCESS;
}