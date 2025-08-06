#include "Window.hpp"

#include <stdexcept>
#include <iostream>


namespace va
{
    Window::Window(uint32_t width, uint32_t height, std::string title) : width{width}, height{height}, title{title}
    {
        InitWindow();
    }

    Window::~Window()
    {
        std::cout << "Window destroyed" << std::endl;
        glfwDestroyWindow(glfwWindow);
        glfwTerminate();
    }

    void Window::InitWindow()
    {
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // GLFW_NO_API to disable generation of OpenGL context
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);   // disable resize, handled manually

        glfwWindow = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
        if (!glfwWindow)
        {
            throw std::runtime_error("failed to create GLFW window!");
        }
    }
}
