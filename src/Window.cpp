#include "Window.hpp"
#include "log/Log.hpp"
#include <stdexcept>
#include <iostream>


namespace m1
{
    void static framebufferResizeCallback(GLFWwindow* window, int width, int height)
    {
        auto w = reinterpret_cast<Window*>(glfwGetWindowUserPointer(window));
        w->FramebufferResized = true;
		if (width == 0 || height == 0)
			w->IsMinimized = true;
        else
			w->IsMinimized = false;
        
    }

    Window::Window(uint32_t width, uint32_t height, std::string title) : width{width}, height{height}, title{title}
    {
        Log::Get().Info("Creating window");
        InitWindow();
    }

    Window::~Window()
    {
        Log::Get().Info("Window destroyed");
        glfwDestroyWindow(glfwWindow);
        glfwTerminate();
    }

    void Window::InitWindow()
    {
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // GLFW_NO_API to disable generation of OpenGL context

        glfwWindow = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
        if (!glfwWindow)
        {
            Log::Get().Error("failed to create GLFW window!");
            throw std::runtime_error("failed to create GLFW window!");
        }

		glfwSetWindowUserPointer(glfwWindow, this); // set a pointer to "this" instance for use in callbacks
		// handle resize explicitly (in case is not notified by driver with VK_ERROR_OUT_OF_DATE_KHR)
        glfwSetFramebufferSizeCallback(glfwWindow, framebufferResizeCallback);
    }

    void Window::createSurface(VkInstance instance, VkSurfaceKHR* surface) const
    {
        if (glfwCreateWindowSurface(instance, glfwWindow, nullptr, surface) != VK_SUCCESS)
        {
            Log::Get().Error("failed to create window surface!");
            throw std::runtime_error("failed to create window surface!");
        }
	}

    

}
