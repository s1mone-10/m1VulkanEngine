#pragma once

#include "Log.hpp" // for VK_CHECK function

//libs
#include "graphics/glm_config.hpp"
#include <vulkan/vulkan.h>
#include <vulkan/vk_enum_string_helper.h> // for VK_CHECK function

#include <memory>
#include <string>
#include <vector>

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
	class Engine;
	class Device;
	class Buffer;
	class Texture;

	void copyBuffer(const Device& device, const Buffer& srcBuffer, const Buffer& dstBuffer, VkDeviceSize size);
	void uploadToDeviceBuffer(const Device& device, const Buffer& dstBuffer, VkDeviceSize size, const void* data);
	void copyImageToImage(VkCommandBuffer cmd, VkImage source, VkImage destination, VkExtent2D srcSize, VkExtent2D dstSize);
	std::unique_ptr<Texture> loadEquirectangularHDRMap(const Engine& engine, const std::string& filePath);
	int getBytesPerPixel(VkFormat format);
	std::vector<char> readFile(const std::string& filename);

	void transitionImageLayout(VkCommandBuffer commandBuffer, VkImage image, uint32_t mipLevels, VkImageLayout currentLayout,
			VkImageLayout newLayout, VkImageAspectFlags aspectMask, uint32_t layerCount = 1);
	void getStageAndAccessMaskForLayout(VkImageLayout layout, VkPipelineStageFlags &stageMask, VkAccessFlags &accessMask);

	glm::mat4 perspectiveProjection(float fov, float aspectRatio, float near, float far);
	glm::mat4 orthoProjection(float left, float right, float bottom, float top, float near, float far);

	[[nodiscard]] VkWriteDescriptorSet initVkWriteDescriptorSet(VkDescriptorSet dstSet, uint32_t dstBinding, VkDescriptorType descriptorType,
		VkDescriptorBufferInfo* pBufferInfo = nullptr, VkDescriptorImageInfo* pImageInfo = nullptr);

	uint32_t computeMipLevels(uint32_t width, uint32_t height);
	VkImageUsageFlags getTextureImageUsageFlags();
}
