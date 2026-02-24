#pragma once

#include "../third_party//imgui//imgui_impl_glfw.h"
#include "../third_party//imgui//imgui_impl_vulkan.h"
#include "Engine.hpp"

namespace m1
{
	class UiModule
	{
	public:
		UiModule(Engine& engine, const Device &device, const Window &window, const SwapChain &swapChain);
		~UiModule();

		// Non-copyable, non-movable
		UiModule(const UiModule&) = delete;
		UiModule& operator=(const UiModule&) = delete;
		UiModule(UiModule&&) = delete;
		UiModule& operator=(UiModule&&) = delete;

		void build();
		void draw(VkCommandBuffer cmd, VkImageView colorImage, VkRect2D renderArea);

	private:
		Engine& _engine;
		const Device& _device;
		VkDescriptorPool _descriptorPool;

		void createDescriptorPool();
		void initImGui(const Window &window, const SwapChain &swapChain);
	};
}