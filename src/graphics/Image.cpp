#include "Image.hpp"
#include "Buffer.hpp"
#include "Device.hpp"
#include "Utils.hpp"
#include "Log.hpp"

#include <stdexcept>

namespace m1
{
    Image::Image(const Device& device, const ImageParams& params)
		: _device(device), _format(params.format), _extent(params.extent), _mipLevels(params.mipLevels), _arrayLayers(params.arrayLayers)
    {
        Log::Get().Info("Creating image from scratch");

        // Image info
        VkImageCreateInfo imageInfo
        {
	        .sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        	.flags		   = params.flags,
	        .imageType     = VK_IMAGE_TYPE_2D,
	        .format        = params.format,
	        .extent        = VkExtent3D{_extent.width, _extent.height, 1},
	        .mipLevels     = params.mipLevels,
	        .arrayLayers   = params.arrayLayers,
	        .samples       = params.samples,
	        .tiling        = params.tiling,
	        .usage         = params.usage,
	        .sharingMode   = VK_SHARING_MODE_EXCLUSIVE, // not shared between multiple queue families
	        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED, // only two possible options: UNDEFINED or PREINITIALIZE
        };

    	// memory allocation info
    	VmaAllocationCreateInfo allocInfo = {};
    	allocInfo.usage = VMA_MEMORY_USAGE_AUTO; // best memory type selected automatically based on usage
    	allocInfo.flags = params.memoryProps;

    	// Create the Image
    	VK_CHECK(vmaCreateImage(_device.getMemoryAllocator(), &imageInfo, &allocInfo, &_vkImage, &_allocation, nullptr));

		// ImageView info
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = _vkImage;
        viewInfo.viewType = params.flags & VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT ? VK_IMAGE_VIEW_TYPE_CUBE : VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = _format;
        viewInfo.subresourceRange.aspectMask = params.aspectMask;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = _mipLevels;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = _arrayLayers;

		// Create the Image View
        VK_CHECK(vkCreateImageView(_device.getVkDevice(), &viewInfo, nullptr, &_imageView));

    	// create an 2D imageView for each layer and mip level
    	if (params.flags & VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT)
    	{
    		_subViews.resize(_arrayLayers * _mipLevels, VK_NULL_HANDLE);

    		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    		uint32_t index {0};

		    for (uint32_t i = 0; i < _arrayLayers; i++)
		    {
		    	// set the layer
			    viewInfo.subresourceRange.baseArrayLayer = i;
			    viewInfo.subresourceRange.layerCount = 1;

		    	for (uint32_t j = 0; j < _mipLevels; j++)
		    	{
		    		// set the mip level
		    		viewInfo.subresourceRange.baseMipLevel = j;
		    		viewInfo.subresourceRange.levelCount = 1;

		    		// create the image view
		    		VkImageView imageView;
		    		VK_CHECK(vkCreateImageView(_device.getVkDevice(), &viewInfo, nullptr, &imageView));
		    		_subViews[index++] = imageView;
		    	}
    		}
    	}
    }
    
    Image::~Image()
    {
        Log::Get().Info("Destroying image");
        vkDestroyImageView(_device.getVkDevice(), _imageView, nullptr);
    	for (auto imageView : _subViews)
    	{
    		if (imageView != VK_NULL_HANDLE)
    			vkDestroyImageView(_device.getVkDevice(), imageView, nullptr);
    	}
    	vmaDestroyImage(_device.getMemoryAllocator(), _vkImage, _allocation);
    }
} // namespace m1
