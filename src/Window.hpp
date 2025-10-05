#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <string>

namespace m1
{
    class Window 
    {
        public:
            Window(uint32_t  width, uint32_t  height, std::string title);
            ~Window();

            // These lines delete the copy constructor and copy assignment operator.
            //Why? To prevent accidental copies of GLFWwindow* resource which could cause double-free, crashes, etc.
            Window(const Window&) = delete;
            Window& operator=(const Window&) = delete;

            bool shouldClose() { return glfwWindowShouldClose(_glfwWindow); }
            void createSurface(VkInstance instance, VkSurfaceKHR* surface) const;
            void getFramebufferSize(int* width, int* height) const { glfwGetFramebufferSize(_glfwWindow, width, height); }
            bool FramebufferResized = false;
			bool IsMinimized = false;
            int getPressedKey();

        private:
            void InitWindow();

            const uint32_t _width;
            const uint32_t _height;
            const std::string _title;
            GLFWwindow* _glfwWindow;
    };
}
