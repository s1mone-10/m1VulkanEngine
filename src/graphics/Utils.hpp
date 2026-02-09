#pragma once

#include "Device.hpp"
#include "Buffer.hpp"

#include <vulkan/vulkan.h>
#include <vulkan/vk_enum_string_helper.h>

#define VK_CHECK(x)			                                                                    \
    do {						                                                                \
        VkResult err = x;			                                                            \
        if (err < 0) {					                                                        \
            m1::Log::Get().Error(std::format("Vulkan Fatal Error: {}", string_VkResult(err)));  \
            abort();																			\
        } else if (err > 0) {																	\
            m1::Log::Get().Info(std::format("Vulkan Status Note: {}", string_VkResult(err)));   \
        }																						\
    } while (0)

namespace m1
{
    class Utils
    {
    public:
        static void copyBuffer(const Device& device, const Buffer& srcBuffer, const Buffer& dstBuffer, VkDeviceSize size);
        static void uploadToDeviceBuffer(const Device& device, const Buffer& dstBuffer, VkDeviceSize size, void* data);
    };
}