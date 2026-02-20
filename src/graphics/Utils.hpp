#pragma once

#include "Device.hpp"
#include "Buffer.hpp"

//libs
#include "graphics/glm_config.hpp"
#include <vulkan/vulkan.h>
#include <vulkan/vk_enum_string_helper.h> // for VK_CHECK function

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

    	static glm::mat4 perspectiveProjection(float fov, float aspectRatio, float near, float far)
    	{
    		auto perspective = glm::perspective(fov, aspectRatio, near, far);

    		// flip the sign of the Y scaling factor because GLM was originally designed for OpenGL, where the Y coordinate of the clip coordinates is inverted
    		perspective[1][1] *= -1;

    		return perspective;
    	}

    	static glm::mat4 orthoProjection(float left, float right, float bottom, float top, float near, float far)
    	{
    		auto ortho = glm::ortho(left, right, bottom, top, near, far);

    		// flip the sign of the Y scaling factor because GLM was originally designed for OpenGL, where the Y coordinate of the clip coordinates is inverted
    		ortho[1][1] *= -1;

    		return ortho;
    	}
    };
}