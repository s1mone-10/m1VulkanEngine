#pragma once

#include <vulkan/vulkan.h>

namespace m1
{
	class Engine;
	class Device;
	class Window;
	class SwapChain;

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
		static bool wantCaptureKeyboard();

	private:
		Engine& _engine;
		VkDescriptorPool _descriptorPool = VK_NULL_HANDLE;

		void createDescriptorPool();
		void initImGui(const Window &window, const SwapChain &swapChain);
	};
}
