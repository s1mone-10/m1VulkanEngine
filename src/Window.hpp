#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <string>

namespace va
{
    class Window 
    {
        const uint32_t  width;
        const uint32_t  height;
        const std::string title;
        GLFWwindow* glfwWindow;

        public:
            Window(uint32_t  width, uint32_t  height, std::string title);
            ~Window();

            // These lines delete the copy constructor and copy assignment operator.
            //Why? To prevent accidental copies of GLFWwindow* resource which could cause double-free, crashes, etc.
            Window(const Window&) = delete;
            Window& operator=(const Window&) = delete;

            bool shouldClose() { return glfwWindowShouldClose(glfwWindow); }
            void createSurface(VkInstance instance, VkSurfaceKHR* surface) const;
            void getFramebufferSize(int* width, int* height) const { glfwGetFramebufferSize(glfwWindow, width, height); }
            bool FramebufferResized = false;
			bool IsMinimized = false;

        private:
            void InitWindow();           
    };
}
