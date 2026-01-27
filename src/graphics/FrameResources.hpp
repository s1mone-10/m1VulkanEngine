#pragma once

#include "Buffer.hpp"
#include "Device.hpp"

// libs
#include <vulkan/vulkan.h>

// std
#include <memory>


namespace m1
{
    struct FrameUbo;
    struct ObjectUbo;

    struct FrameResources
    {
    	FrameResources(const FrameUbo& frameUbo, std::unique_ptr<Buffer> frameUboBuffer, const ObjectUbo& objectUbo,
    		std::unique_ptr<Buffer> objectUboBuffer, VkDescriptorSet frameDescriptorSet) :
				frameUbo(frameUbo), frameUboBuffer(std::move(frameUboBuffer)), objectUbo(objectUbo),
				objectUboBuffer(std::move(objectUboBuffer)), descriptorSet(frameDescriptorSet)
    	{
    	}

        // non-copyable
        FrameResources(const FrameResources&) = delete;
        FrameResources& operator=(const FrameResources&) = delete;

        FrameUbo frameUbo;
        std::unique_ptr<Buffer> frameUboBuffer;

        ObjectUbo objectUbo;
        std::unique_ptr<Buffer> objectUboBuffer;
    	VkDescriptorSet descriptorSet = VK_NULL_HANDLE;

    	// Material dynamic ubo buffer. Contains data of all materials.
        std::unique_ptr<Buffer> materialDynUboBuffer;
    };
}
