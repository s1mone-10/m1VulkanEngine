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

    struct FrameData
    {
    	FrameData(const FrameUbo& frameUbo, std::unique_ptr<Buffer> frameUboBuffer, const ObjectUbo& objectUbo,
    		std::unique_ptr<Buffer> objectUboBuffer, VkDescriptorSet frameDescriptorSet, VkFence drawFence,
    		VkCommandBuffer drawSceneCmdBuffer) :
				frameUbo(frameUbo), frameUboBuffer(std::move(frameUboBuffer)), objectUbo(objectUbo),
				objectUboBuffer(std::move(objectUboBuffer)), descriptorSet(frameDescriptorSet),
    			drawCmdExecutedFence(drawFence), drawSceneCmdBuffer(drawSceneCmdBuffer)
    	{
    	}

        // non-copyable
        FrameData(const FrameData&) = delete;
        FrameData& operator=(const FrameData&) = delete;

    	// buffers
        FrameUbo frameUbo;
        std::unique_ptr<Buffer> frameUboBuffer;

        ObjectUbo objectUbo;
        std::unique_ptr<Buffer> objectUboBuffer;

    	std::unique_ptr<Buffer> particleSSboBuffer;

        std::unique_ptr<Buffer> materialDynUboBuffer; // contains data of all materials

    	// descriptor set
    	VkDescriptorSet descriptorSet = VK_NULL_HANDLE;

    	// synchronization objects
    	VkFence drawCmdExecutedFence, computeCmdExecutedFence = VK_NULL_HANDLE;
    	VkSemaphore computeCmdExecutedSem = VK_NULL_HANDLE;

    	// command buffers
    	VkCommandBuffer drawSceneCmdBuffer, computeCmdBuffer = VK_NULL_HANDLE;
    };
}
