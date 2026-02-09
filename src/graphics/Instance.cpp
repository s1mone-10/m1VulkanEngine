#include "Instance.hpp"
#include "Window.hpp"
#include "log/Log.hpp"
#include "Utils.hpp"

#include <stdexcept>
#include <vector>
#include <cstring>
#include <iostream>

namespace m1
{
    Instance::Instance()
    {
        Log::Get().Info("Creating instance");
        createInstance();
    }

    Instance::~Instance()
    {
	    vkDestroyInstance(_vkInstance, nullptr);
        Log::Get().Info("Instance destroyed");
    }

    void Instance::createInstance()
    {
        // Application Info
        // Technically optional, but it may provide some useful information to the driver in order to optimize the application
        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO; // many structs in Vulkan require you to explicitly specify the type in the sType member
        appInfo.pApplicationName = "Vulkan App";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "No Engine";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION;

		// Instance Info
        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;
        auto extensions = getRequiredExtensions();
        createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        createInfo.ppEnabledExtensionNames = extensions.data();

        // Create the Vulkan instance
    	VK_CHECK(vkCreateInstance(&createInfo, nullptr, &_vkInstance));
    }

    std::vector<const char*> Instance::getRequiredExtensions()
    {
        // GLFW has a handy built-in function that returns the extension(s) needed to interface with the Window system
        uint32_t glfwExtensionCount = 0;
        const char** glfwExtensions;
        glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

        std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

        return extensions;
    }
} // namespace m1
