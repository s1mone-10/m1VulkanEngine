#pragma once

#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"
#include "Engine.hpp"

namespace m1
{
	class UiModule
	{
	public:
		UiModule(Engine& engine, const Window &window, const SwapChain &swapChain);
		~UiModule();

		// Non-copyable, non-movable
		UiModule(const UiModule&) = delete;
		UiModule& operator=(const UiModule&) = delete;
		UiModule(UiModule&&) = delete;
		UiModule& operator=(UiModule&&) = delete;

		void build() const;
		void draw(VkCommandBuffer cmdBuffer, VkImageView colorImage, VkRect2D renderArea);

	private:
		Engine& _engine;
		VkDescriptorPool _descriptorPool = VK_NULL_HANDLE;

		void createDescriptorPool();
		void initImGui(const Window &window, const SwapChain &swapChain);
	};
}