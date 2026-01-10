#pragma once

#include <vulkan/vulkan.h>
#include <vector>

namespace m1
{
    class Instance
    {
    public:
        Instance();
        ~Instance();

        // Non-copyable, non-movable
        Instance(const Instance&) = delete;
        Instance& operator=(const Instance&) = delete;
        Instance(Instance&&) = delete;
        Instance& operator=(Instance&&) = delete;

        VkInstance getVkInstance() const { return _vkInstance; }

    private:
        void createInstance();
        std::vector<const char*> getRequiredExtensions();

        VkInstance _vkInstance = VK_NULL_HANDLE;
        VkDebugUtilsMessengerEXT _debugMessenger = VK_NULL_HANDLE;
    };
}
