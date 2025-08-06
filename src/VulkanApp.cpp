#include <stdexcept>

#include "VulkanApp.hpp"
#include <vector>
#include <cstring>

namespace
{
    const std::vector<const char *> validationLayers = {
        "VK_LAYER_KHRONOS_validation" // standard diagnostics layers provided by the Vulkan SDK
    };

    // enable validationLayers only at debug time
    const bool enableValidationLayers =
#ifdef NDEBUG
        false;
#else
        true;
#endif

    bool checkValidationLayerSupport()
    {
        uint32_t layerCount;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

        std::vector<VkLayerProperties> availableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

        for (const char *layerName : validationLayers)
        {
            bool layerFound = false;

            for (const auto &layerProperties : availableLayers)
            {
                if (strcmp(layerName, layerProperties.layerName) == 0)
                {
                    layerFound = true;
                    break;
                }
            }

            if (!layerFound)
            {
                return false;
            }
        }

        return false;
    }
}

namespace va
{
    void VulkanApp::run()
    {
        initVulkan();
        mainLoop();
        cleanup();
    }

    void VulkanApp::initVulkan()
    {
        createInstance();
    }

    void VulkanApp::mainLoop()
    {
        while (!window.shouldClose())
        {
            glfwPollEvents();
        }
    }

    void VulkanApp::cleanup()
    {
        vkDestroyInstance(instance, nullptr);
    }

    void VulkanApp::createInstance()
    {
        // if (enableValidationLayers && !checkValidationLayerSupport()) {
        //     throw std::runtime_error("Validation layers requested, but not available!");
        // }


        // technically optional, but it may provide some useful information to the driver in order to optimize the application
        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO; // many structs in Vulkan require you to explicitly specify the type in the sType member
        appInfo.pApplicationName = "Vulkan App";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "No Engine";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_0;

        // GLFW has a handy built-in function that returns the extension(s) needed to interface with the Window system
        uint32_t glfwExtensionCount = 0;
        const char **glfwExtensions;
        glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;
        createInfo.enabledExtensionCount = glfwExtensionCount;
        createInfo.ppEnabledExtensionNames = glfwExtensions;
        if (enableValidationLayers)
        {
            createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            createInfo.ppEnabledLayerNames = validationLayers.data();
        }
        else
            createInfo.enabledLayerCount = 0;

        VkResult result = vkCreateInstance(&createInfo, nullptr, &instance);
        if (result != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create instance!");
        }
    }

}