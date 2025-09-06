#include "Device.hpp"
#include "Instance.hpp"
#include "SwapChain.hpp" // For SwapChain::querySwapChainSupport
#include "log/Log.hpp"
#include <stdexcept>
#include <set>
#include <iostream>

namespace m1
{

	Device::Device(const Window& window)
    {
        Log::Get().Info("Creating device");
        createSurface(window);
        pickPhysicalDevice();
        createLogicalDevice();
		pickDeviceQueues();
    }

    Device::~Device()
    {
		// physical device is implicitly destroyed when the VkInstance is destroyed
        // Device queues are implicitly destroyed when the device is destroyed
        vkDestroyDevice(_vkDevice, nullptr);
        vkDestroySurfaceKHR(_instance.getVkInstance(), _surface, nullptr);
        Log::Get().Info("Device destroyed");
    }

    VkDeviceMemory Device::allocateMemory(VkMemoryRequirements memRequirements, VkMemoryPropertyFlags properties) const
    {
        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size; // maybe different from bufferInfo.size
        allocInfo.memoryTypeIndex = findMemoryTypeIndex(memRequirements.memoryTypeBits, properties);

        // Allocate the memory
		VkDeviceMemory deviceMemory;
        if (vkAllocateMemory(_vkDevice, &allocInfo, nullptr, &deviceMemory) != VK_SUCCESS)
        {
            Log::Get().Error("failed to allocate device memory!");
            throw std::runtime_error("failed to allocate device memory!");
        }

        return deviceMemory;
    }

    VkFormat Device::findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) const
    {
        for (VkFormat format : candidates)
        {
            VkFormatProperties props;
            vkGetPhysicalDeviceFormatProperties(_physicalDevice, format, &props);

            if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features)
            {
                return format;
            }
            else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features)
            {
                return format;
            }
        }
        throw std::runtime_error("failed to find supported format!");
    }

    void Device::createSurface(const Window& window)
    {
        Log::Get().Info("Creating surface");
        window.createSurface(_instance.getVkInstance(), &_surface);
    }

    void Device::pickPhysicalDevice()
    {
        Log::Get().Info("Picking physical device");
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(_instance.getVkInstance(), &deviceCount, nullptr);

        if (deviceCount == 0)
        {
            Log::Get().Error("Failed to find GPUs with Vulkan support!");
            throw std::runtime_error("Failed to find GPUs with Vulkan support!");
        }
        
        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(_instance.getVkInstance(), &deviceCount, devices.data());

        // TODO: Instead of just checking if a device is suitable, gives each device a score and pick the highest one

        for (const auto device : devices)
        {
            if (isDeviceSuitable(device))
            {
                _physicalDevice = device;
                Log::Get().Info("Picked physical device");
                return;
            }
        }

        Log::Get().Error("No suitable GPU");
        throw std::runtime_error("No suitable GPU");
    }

    void Device::createLogicalDevice()
    {
        Log::Get().Info("Creating logical device");
        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
        // Gets unique queue indices (duplicates are automatically discarded)
        std::set<uint32_t> uniqueQueueFamilies = { _queueFamilies.graphicsFamily.value(), _queueFamilies.presentFamily.value() };

        // Queue info
        float queuePriority = 1.0f;
        for (uint32_t familyIndex : uniqueQueueFamilies)
        {
            // set the number of queues for each unique queue family
            VkDeviceQueueCreateInfo queueCreateInfo{};
            queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfo.queueFamilyIndex = familyIndex;
            queueCreateInfo.queueCount = 1;
            queueCreateInfo.pQueuePriorities = &queuePriority;
            queueCreateInfos.push_back(queueCreateInfo);
        }

        // Device features
        VkPhysicalDeviceFeatures deviceFeatures{};
		deviceFeatures.samplerAnisotropy = VK_TRUE; // enable anisotropic filtering

        // Device info
        VkDeviceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
        createInfo.pQueueCreateInfos = queueCreateInfos.data();
        createInfo.pEnabledFeatures = &deviceFeatures;
        createInfo.enabledExtensionCount = static_cast<uint32_t>(_requiredExtensions.size());
        createInfo.ppEnabledExtensionNames = _requiredExtensions.data();

        if (Instance::enableValidationLayers)
        {
            createInfo.enabledLayerCount = static_cast<uint32_t>(_instance.validationLayers.size());
            createInfo.ppEnabledLayerNames = _instance.validationLayers.data();
        }
        else
        {
            createInfo.enabledLayerCount = 0;
        }

        // Create logical device
        if (vkCreateDevice(_physicalDevice, &createInfo, nullptr, &_vkDevice) != VK_SUCCESS)
        {
            Log::Get().Error("failed to create logical device!");
            throw std::runtime_error("failed to create logical device!");
        }
    }

    void Device::pickDeviceQueues()
    {
        vkGetDeviceQueue(_vkDevice, _queueFamilies.graphicsFamily.value(), 0, &_graphicsQueue);
        vkGetDeviceQueue(_vkDevice, _queueFamilies.presentFamily.value(), 0, &_presentQueue);
    }

    bool Device::isDeviceSuitable(VkPhysicalDevice device)
    {
        VkPhysicalDeviceProperties deviceProperties;
        VkPhysicalDeviceFeatures deviceFeatures;
        vkGetPhysicalDeviceProperties(device, &deviceProperties);
        vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

        if(!deviceFeatures.samplerAnisotropy)
			return false;

        // check queue families
        _queueFamilies = findQueueFamilies(device);
        if (!_queueFamilies.isComplete())
            return false;

        // check extensions support
        if (!checkDeviceExtensionSupport(device))
            return false;

        // check swapChain support
        auto swapChainProperties = getSwapChainProperties(device);
        if (swapChainProperties.formats.empty() || swapChainProperties.presentModes.empty())
            return false;

        return true;
    }

    bool Device::checkDeviceExtensionSupport(VkPhysicalDevice device)
    {
        uint32_t extensionCount;
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

        std::vector<VkExtensionProperties> availableExtensions(extensionCount);
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

        std::set<std::string> requiredExtensions(_requiredExtensions.begin(), _requiredExtensions.end());

        for (const auto& extension : availableExtensions)
        {
            requiredExtensions.erase(extension.extensionName);
        }

        return requiredExtensions.empty();
    }

    QueueFamilyIndices Device::findQueueFamilies(VkPhysicalDevice device)
    {
        QueueFamilyIndices indices;

        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

        int i = 0;
        for (const auto& queueFamily : queueFamilies)
        {
            // graphicsFamily
            if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
            {
                indices.graphicsFamily = i;
            }

            // presentFamily
            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, _surface, &presentSupport);
            if (presentSupport)
            {
                indices.presentFamily = i; // very likely the same of graphicsFamily. Better for performance if they are the same
            }

            if (indices.isComplete())
            {
                break;
            }

            i++;
        }

        return indices;
    }

    SwapChainProperties Device::getSwapChainProperties(const VkPhysicalDevice device) const
    {
        SwapChainProperties details;

        // cababilities
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, _surface, &details.capabilities);

        // formats
        uint32_t formatCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, _surface, &formatCount, nullptr);
        if (formatCount != 0)
        {
            details.formats.resize(formatCount);
            vkGetPhysicalDeviceSurfaceFormatsKHR(device, _surface, &formatCount, details.formats.data());
        }

        // present modes
        uint32_t presentModeCount;
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, _surface, &presentModeCount, nullptr);
        if (presentModeCount != 0)
        {
            details.presentModes.resize(presentModeCount);
            vkGetPhysicalDeviceSurfacePresentModesKHR(device, _surface, &presentModeCount, details.presentModes.data());
        }
        return details;
    }

    /// <summary>
    /// Finds a suitable memory type index based on a type filter and desired memory properties.
    /// </summary>
    /// <param name="typeFilter">A bitmask specifying the acceptable memory types.</param>
    /// <param name="properties">Flags specifying the desired memory properties.</param>
    /// <returns>The index of a suitable memory type that matches the filter and properties.</returns>
    uint32_t Device::findMemoryTypeIndex(uint32_t typeFilter, VkMemoryPropertyFlags properties) const
    {
        // GPU can offer different types of memory, each type varies in terms of allowed operations and performance characteristics

        // Get the memory properties of the physical device
        VkPhysicalDeviceMemoryProperties memProperties;
        vkGetPhysicalDeviceMemoryProperties(_physicalDevice, &memProperties);

        // Find a memory type that matches the type filter and has the desired properties
        for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
        {
            if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
            {
                return i;
            }
        }

        Log::Get().Error("failed to find suitable memory type!");
        throw std::runtime_error("failed to find suitable memory type!");
    }
} // namespace m1
