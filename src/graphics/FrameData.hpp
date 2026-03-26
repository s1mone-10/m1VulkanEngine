#pragma once

#include "Buffer.hpp"

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
    	FrameData(std::unique_ptr<Buffer> frameUboBuffer, std::unique_ptr<Buffer> objectUboBuffer, VkDescriptorSet frameDescriptorSet,
    		VkFence drawFence, VkCommandBuffer drawSceneCmdBuffer) :
				frameUboBuffer(std::move(frameUboBuffer)), objectUboBuffer(std::move(objectUboBuffer)), frameDescriptorSet(frameDescriptorSet),
    			drawCmdExecutedFence(drawFence), drawSceneCmdBuffer(drawSceneCmdBuffer)
    	{
    	}

        // non-copyable
        FrameData(const FrameData&) = delete;
        FrameData& operator=(const FrameData&) = delete;

    	// buffers
        std::unique_ptr<Buffer> frameUboBuffer;
        std::unique_ptr<Buffer> objectUboBuffer;
    	std::unique_ptr<Buffer> particleSSboBuffer;

        std::unique_ptr<Buffer> materialPhongDynUboBuffer; // contains data of all materials
        std::unique_ptr<Buffer> materialPbrDynUboBuffer;

    	// descriptor set
    	VkDescriptorSet frameDescriptorSet = VK_NULL_HANDLE;
    	VkDescriptorSet skyBoxDescriptorSet = VK_NULL_HANDLE;
    	VkDescriptorSet computeParticleDescriptorSet = VK_NULL_HANDLE;

    	// synchronization objects
    	VkFence drawCmdExecutedFence, computeCmdExecutedFence = VK_NULL_HANDLE;
    	VkSemaphore computeCmdExecutedSem = VK_NULL_HANDLE;

    	// command buffers
    	VkCommandBuffer drawSceneCmdBuffer, computeCmdBuffer = VK_NULL_HANDLE;
    };
}
