#include "Window.hpp"
#include "log/Log.hpp"
#include "graphics/Utils.hpp"
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

    Window::Window(uint32_t width, uint32_t height, std::string title) : _width{width}, _height{height}, _title{title}
    {
        Log::Get().Info("Creating window");
        InitWindow();
    }

    Window::~Window()
    {
        Log::Get().Info("Window destroyed");
        glfwDestroyWindow(_glfwWindow);
        glfwTerminate();
    }

    int Window::getPressedKey()
    {
        for (int key = GLFW_KEY_SPACE; key <= GLFW_KEY_LAST; ++key)
        {
            if (glfwGetKey(_glfwWindow, key) == GLFW_PRESS)
                return key;
        }
    }

    void Window::InitWindow()
    {
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // GLFW_NO_API to disable generation of OpenGL context

        _glfwWindow = glfwCreateWindow(_width, _height, _title.c_str(), nullptr, nullptr);
        if (!_glfwWindow)
        {
            Log::Get().Error("failed to create GLFW window!");
            throw std::runtime_error("failed to create GLFW window!");
        }

		glfwSetWindowUserPointer(_glfwWindow, this); // set a pointer to "this" instance for use in callbacks
		// handle resize explicitly (in case is not notified by driver with VK_ERROR_OUT_OF_DATE_KHR)
        glfwSetFramebufferSizeCallback(_glfwWindow, framebufferResizeCallback);
    }

    void Window::createSurface(VkInstance instance, VkSurfaceKHR* surface) const
    {
    	VK_CHECK(glfwCreateWindowSurface(instance, _glfwWindow, nullptr, surface));
	}

    

}
