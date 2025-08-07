#include <stdexcept>

#include "VulkanApp.hpp"
#include <vector>
#include <cstring>
#include <iostream>

namespace
{
    const std::vector<const char *> validationLayers = {
        "VK_LAYER_KHRONOS_validation" // standard diagnostics layers provided by the Vulkan SDK
    };

    // enable validationLayers only at debug time
    bool enableValidationLayers =
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
        pickPhysicalDevice();
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
        if (enableValidationLayers && !checkValidationLayerSupport())
        {
            enableValidationLayers = false; // TODO check why is not available
            std::cerr << "Validation layers requested, but not available!" << std::endl;
        }

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

    void VulkanApp::pickPhysicalDevice()
    {
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

        if (deviceCount == 0)
            throw std::runtime_error("Failed to find GPUs with Vulkan support!");

        std::vector<VkPhysicalDevice> physicalDevices(deviceCount);
        vkEnumeratePhysicalDevices(instance, &deviceCount, physicalDevices.data());

        for (const auto device : physicalDevices)
        {
            if (isDeviceSuitable(device))
            {
                physicalDevice = device;
                return;
            }
        }

        throw std::runtime_error("No suitable GPU");
    }

    bool VulkanApp::isDeviceSuitable(VkPhysicalDevice device)
    {
        VkPhysicalDeviceProperties deviceProperties;
        VkPhysicalDeviceFeatures deviceFeatures;
        vkGetPhysicalDeviceProperties(device, &deviceProperties);
        vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

        // eventually check for properties or features

        auto queuefamilyIndices = findQueueFamilies(device);


        return queuefamilyIndices.isComplete();
    }

    QueueFamilyIndices VulkanApp::findQueueFamilies(VkPhysicalDevice device)
    {
        QueueFamilyIndices qf;

        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

        int i = 0;
        for (const auto &queueFamily : queueFamilies)
        {
            if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
            {
                qf.graphicsFamily = i;
            }

            i++;
        }

        return qf;
    }
}