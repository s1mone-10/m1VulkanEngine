#pragma once

#include "Device.hpp"
#include "Buffer.hpp"

#include <vulkan/vulkan.h>

namespace m1
{
    class Utils
    {
    public:
        static void copyBuffer(const Device& device, const Buffer& srcBuffer, const Buffer& dstBuffer, VkDeviceSize size);
        static void uploadToDeviceBuffer(const Device& device, const Buffer& dstBuffer, VkDeviceSize size, void* data);
    };
}