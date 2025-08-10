#include <stdexcept>

#include "VulkanApp.hpp"
#include <cstring>
#include <iostream>
#include <set>
#include <algorithm> // Necessary for std::clamp

namespace
{
    const std::vector<const char *> validationLayers = {
        "VK_LAYER_KHRONOS_validation" // standard diagnostics layers provided by the Vulkan SDK
    };

    const std::vector<const char*> deviceExtensions = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME // is an extension because tied with the window, so the Operating System, not the Vulkan API
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
    }

    VulkanApp::~VulkanApp()
    {
        std::cout << "VulkanApp destroyed" << std::endl;

        for (auto imageView : swapChainImageViews) 
            vkDestroyImageView(device, imageView, nullptr);
        vkDestroySwapchainKHR(device, swapChain, nullptr);
        vkDestroyDevice(device, nullptr);
		vkDestroySurfaceKHR(instance, surface, nullptr);
        vkDestroyInstance(instance, nullptr);
    }

    void VulkanApp::initVulkan()
    {
        createInstance();
        //setupDebugMessenger(); // TODO
        window.createSurface(instance, &surface); // needs to be created right after the instance creation
        pickPhysicalDevice();
        createLogicalDevice();
		createSwapChain();
        createImageViews();
		createGraphicsPipeline();
    }

    void VulkanApp::mainLoop()
    {
        while (!window.shouldClose())
        {
            glfwPollEvents();
        }
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
        const char** glfwExtensions;
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


        // check extensions support
        if(!checkDeviceExtensionSupport(device)) {
            return false;
		}

        // check swapChain support
		SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
        if(swapChainSupport.formats.empty() || swapChainSupport.presentModes.empty()) {
            return false;
		}

        initQueueFamilies(device);

        return qfIndices.isComplete();
    }

    bool VulkanApp::checkDeviceExtensionSupport(VkPhysicalDevice device) {
        uint32_t extensionCount;
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

        std::vector<VkExtensionProperties> availableExtensions(extensionCount);
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

        std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

        for (const auto& extension : availableExtensions) {
            requiredExtensions.erase(extension.extensionName);
        }

        return requiredExtensions.empty();
    }

    void VulkanApp::initQueueFamilies(VkPhysicalDevice device) {
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

        int i = 0;
        for (const auto& queueFamily : queueFamilies) {
            // graphicsFamily
            if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
            {
                qfIndices.graphicsFamily = i;
            }

            // presentFamily
            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
            if (presentSupport)
                qfIndices.presentFamily = i; // very likely the same of graphicsFamily. Better for performance if they are the same

            if (qfIndices.isComplete()) 
                break;
            

            i++;
        }
    }

    void VulkanApp::createLogicalDevice()
    {
        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;

        // Gets unique queue indices (duplicates are automatically discarded)
        std::set<uint32_t> uniqueQueueFamilies =
        { 
            qfIndices.graphicsFamily.value(), 
            qfIndices.presentFamily.value() 
        };

        // Queue info
        float queuePriority = 1.0f;
        for (uint32_t queueFamily : uniqueQueueFamilies) {
            // set the number of queues for each unique queue family
            VkDeviceQueueCreateInfo queueCreateInfo{};
            queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfo.queueFamilyIndex = queueFamily;
            queueCreateInfo.queueCount = 1;
            queueCreateInfo.pQueuePriorities = &queuePriority;
            queueCreateInfos.push_back(queueCreateInfo);
        }

        VkPhysicalDeviceFeatures deviceFeatures{};

        // Device info
        VkDeviceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		// Queue creation info
        createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
        createInfo.pQueueCreateInfos = queueCreateInfos.data();
		// Features
        createInfo.pEnabledFeatures = &deviceFeatures;
		// Extensions
        createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
        createInfo.ppEnabledExtensionNames = deviceExtensions.data();

        // Instantiate logical device
        if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS) {
            throw std::runtime_error("failed to create logical device!");
        }

        VkQueue graphicsQueue; //Device queues are implicitly cleaned up when the device is destroyed
		vkGetDeviceQueue(device, qfIndices.graphicsFamily.value(), 0, &graphicsQueue);

		VkQueue presentQueue;
        vkGetDeviceQueue(device, qfIndices.presentFamily.value(), 0, &presentQueue);
        
       
    }

    void VulkanApp::createSwapChain() {
        VulkanApp::SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physicalDevice);

        VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
        swapChainImageFormat = surfaceFormat.format;
        VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
        VkExtent2D swapChainExtent = chooseSwapExtent(swapChainSupport.capabilities);

        // recommended to request at least one more image than the minimum
        // to avoid waitin for the driver to complete internal operations before we can acquire another image to render to
        uint32_t minImageCount = swapChainSupport.capabilities.minImageCount + 1;

        if (minImageCount > swapChainSupport.capabilities.maxImageCount &&
            swapChainSupport.capabilities.maxImageCount != 0) // 0 is a special value that means that there is no maximum
        {
            minImageCount = swapChainSupport.capabilities.maxImageCount;
        }

        // set info
        VkSwapchainCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = surface;
        createInfo.minImageCount = minImageCount;
        createInfo.imageFormat = surfaceFormat.format;
        createInfo.imageColorSpace = surfaceFormat.colorSpace;
        createInfo.presentMode = presentMode;
		createInfo.clipped = VK_TRUE; // don't care about pixels that are not visible to the user, for example because another window is in front of them
        createInfo.imageExtent = swapChainExtent;
        createInfo.imageArrayLayers = 1; // This is always 1 unless you are developing a stereoscopic 3D application
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; // rendering direct on the image

        if (qfIndices.graphicsFamily != qfIndices.presentFamily) 
        {
            uint32_t queueFamilyIndices[] = { qfIndices.graphicsFamily.value(), qfIndices.presentFamily.value() };

            createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            createInfo.queueFamilyIndexCount = 2;
            createInfo.pQueueFamilyIndices = queueFamilyIndices;
        }
        else 
        {
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE; // best performance
            createInfo.queueFamilyIndexCount = 0; // Optional
            createInfo.pQueueFamilyIndices = nullptr; // Optional
        }
		createInfo.preTransform = swapChainSupport.capabilities.currentTransform; // don't transform the image
		createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR; // no blending with other windows
        createInfo.oldSwapchain = VK_NULL_HANDLE; // Optional, used for recreating the swap chain
        
        // create swap chain
        if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapChain) != VK_SUCCESS) {
            throw std::runtime_error("failed to create swap chain!");
        }

        // get images from the swap chain
        uint32_t count;
        vkGetSwapchainImagesKHR(device, swapChain, &count, nullptr);
        swapChainImages.resize(minImageCount);
        vkGetSwapchainImagesKHR(device, swapChain, &minImageCount, swapChainImages.data());
    }

    VulkanApp::SwapChainSupportDetails VulkanApp::querySwapChainSupport(VkPhysicalDevice device) {
        VulkanApp::SwapChainSupportDetails details;

		// cababilities
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

        // formats
        uint32_t count;
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &count, nullptr);

        if (count != 0) {
            details.formats.resize(count);
            vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &count, details.formats.data());
        }

		// present modes
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &count, nullptr);

        if(count != 0) {
            details.presentModes.resize(count);
            vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &count, details.presentModes.data());
		}

        return details;
    }

    VkSurfaceFormatKHR VulkanApp::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) 
    {
        for (const auto& availableFormat : availableFormats) 
        {
            if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) 
            {
                return availableFormat;
            }
        }

		// eventually add logic to choose a different format if the preferred one is not available
        return availableFormats[0];
    }

    VkPresentModeKHR VulkanApp::chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) 
    {
        // https://vulkan-tutorial.com/en/Drawing_a_triangle/Presentation/Swap_chain

        for (const auto& availablePresentMode : availablePresentModes) {
            if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
                return availablePresentMode;
            }
        }

        return VK_PRESENT_MODE_FIFO_KHR;
    }

    VkExtent2D VulkanApp::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) 
    {
        if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
            return capabilities.currentExtent;
        }
        else {
            int width, height;
			window.getFramebufferSize(&width, &height);


            VkExtent2D actualExtent = {
                static_cast<uint32_t>(width),
                static_cast<uint32_t>(height)
            };

            actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
            actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

            return actualExtent;
        }
    }

    void VulkanApp::createImageViews()
    {
        uint32_t size = swapChainImages.size();
        swapChainImageViews.resize(size);

        for (size_t i = 0; i < size; i++)
        {
            VkImageViewCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            createInfo.image = swapChainImages[i];
            createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            createInfo.format = swapChainImageFormat;
            createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            createInfo.subresourceRange.baseMipLevel = 0;
            createInfo.subresourceRange.levelCount = 1;
            createInfo.subresourceRange.baseArrayLayer = 0;
            createInfo.subresourceRange.layerCount = 1;

            if (vkCreateImageView(device, &createInfo, nullptr, &swapChainImageViews[i]) != VK_SUCCESS) {
                throw std::runtime_error("failed to create image views!");
            }
        }
    }

    void VulkanApp::createGraphicsPipeline() {

    }
}