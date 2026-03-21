#pragma once
#include <vector>

#include <vulkan/vulkan.h>

namespace m1
{
	class Renderer
	{
	public:
		static void beginRendering(VkCommandBuffer cmdBuffer, VkRect2D renderArea, uint32_t colorAttachmentCount, VkRenderingAttachmentInfo* pColorAttachments, VkRenderingAttachmentInfo* pDepthAttachment);
		static void endRendering(VkCommandBuffer cmdBuffer);
		static void setDynamicStates(VkCommandBuffer cmdBuffer, VkExtent2D extent);
		static VkRenderingAttachmentInfo createColorAttachment(VkImageView imageView);
		static VkRenderingAttachmentInfo createDepthAttachment(VkImageView imageView);
	};
}
