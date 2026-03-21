#include "UiModule.hpp"
#include "Queue.hpp"
#include "Renderer.hpp"
#include "Utils.hpp"

static void check_vk_result(VkResult err)
{
	if (err < 0)
	{
		m1::Log::Get().Error(std::format("Vulkan Fatal Error: {}", string_VkResult(err)));
		abort();
	}
	if (err > 0)
	{
		m1::Log::Get().Info(std::format("Vulkan Status Note: {}", string_VkResult(err)));
	}
}

namespace m1
{
	UiModule::UiModule(Engine& engine, const Device& device, const Window& window, const SwapChain& swapChain)
		: _engine(engine), _device(device)
	{
		createDescriptorPool();
		initImGui(window, swapChain);
	}

	UiModule::~UiModule()
	{
		ImGui_ImplVulkan_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();

		vkDestroyDescriptorPool(_device.getVkDevice(), _descriptorPool, nullptr);
	}

	void UiModule::build()
	{
		// Start the Dear ImGui frame
		ImGui_ImplVulkan_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		ImGui::Begin("Engine controls");

		bool enableMsaa = _engine.getMsaaEnabled();
		if (ImGui::Checkbox("MSAA", &enableMsaa))
		{
			_engine.setMsaaEnabled(enableMsaa);
		}

		bool particlesEnabled = _engine.getParticlesEnabled();
		if (ImGui::Checkbox("Particles", &particlesEnabled))
		{
			_engine.setParticlesEnabled(particlesEnabled);
		}

		bool shadowsEnabled = _engine.getShadowsEnabled();
		if (ImGui::Checkbox("Shadows", &shadowsEnabled))
		{
			_engine.setShadowsEnabled(shadowsEnabled);
		}

		int lightingMode = _engine.getLightingType() == LightingType::BlinnPhong ? 0 : 1;
		const char* lightingItems[] = {"Blinn-Phong", "PBR"};
		if (ImGui::Combo("Lighting model", &lightingMode, lightingItems, IM_ARRAYSIZE(lightingItems)))
		{
			_engine.setLightingType(lightingMode == 0 ? LightingType::BlinnPhong : LightingType::Pbr);
		}

		ImGui::TextUnformatted("Note: shadow toggle is config-only right now.");
		ImGui::End();

		ImGui::Render();
	}

	void UiModule::draw(VkCommandBuffer cmdBuffer, VkImageView colorImage, VkRect2D renderArea)
	{
		// set the color attachment
		VkRenderingAttachmentInfo colorAttachment = Renderer::createColorAttachment(colorImage);
		colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD; // don't clear the color buffer, we want to render the UI on top of the existing scene

		// begin rendering
		Renderer::beginRendering(cmdBuffer, renderArea, 1, &colorAttachment, nullptr);

		// render gui
		ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmdBuffer);

		// end rendering
		Renderer::endRendering(cmdBuffer);
	}

	void UiModule::createDescriptorPool()
	{
		VkDescriptorPoolSize pool_sizes[] =
		{
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, IMGUI_IMPL_VULKAN_MINIMUM_IMAGE_SAMPLER_POOL_SIZE },
		};
		VkDescriptorPoolCreateInfo pool_info = {};
		pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
		pool_info.maxSets = 0;
		for (VkDescriptorPoolSize& pool_size : pool_sizes)
			pool_info.maxSets += pool_size.descriptorCount;
		pool_info.poolSizeCount = static_cast<uint32_t>(IM_COUNTOF(pool_sizes));
		pool_info.pPoolSizes = pool_sizes;
		VK_CHECK(vkCreateDescriptorPool(_device.getVkDevice(), &pool_info, nullptr, &_descriptorPool));
	}

	void UiModule::initImGui(const Window& window, const SwapChain& swapChain)
	{
		// Setup Dear ImGui context
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO(); (void)io;
		io.ConfigNavMoveSetMousePos = true; // Enable mouse control
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls

		// Setup Dear ImGui style
		ImGui::StyleColorsDark();

		// Setup scaling
		ImGuiStyle& style = ImGui::GetStyle();
		style.FontScaleMain = 1.5f;
		// style.ScaleAllSizes(main_scale); // Bake a fixed style scale. (until we have a solution for dynamic style scaling, changing this requires resetting Style + calling this again)
		// style.FontScaleDpi = main_scale; // Set initial font scale. (in docking branch: using io.ConfigDpiScaleFonts=true automatically overrides this for every window depending on the current monitor)

		VkFormat colorFormat = swapChain.getSwapChainImageFormat();

		// Setup Platform/Renderer backends
		ImGui_ImplGlfw_InitForVulkan(window.getGlfwWindow(), true);
		ImGui_ImplVulkan_InitInfo initInfo = {};
		initInfo.ApiVersion = Instance::VK_API_VERSION;
		initInfo.Instance = _device.getVkInstance();
		initInfo.PhysicalDevice = _device.getVkPhysicalDevice();
		initInfo.Device = _device.getVkDevice();
		initInfo.QueueFamily = _device.getQueueFamilyIndices().graphicsFamily.value();
		initInfo.Queue = _device.getGraphicsQueue().getVkQueue();
		//initInfo.PipelineCache = g_PipelineCache;
		initInfo.DescriptorPool = _descriptorPool;
		initInfo.MinImageCount = static_cast<uint32_t>(swapChain.getImageCount());
		initInfo.ImageCount = static_cast<uint32_t>(swapChain.getImageCount());
		//initInfo.Allocator = nullptr;
		initInfo.PipelineInfoMain.RenderPass = nullptr;
		initInfo.PipelineInfoMain.Subpass = 0;
		initInfo.UseDynamicRendering = true;
		initInfo.PipelineInfoMain.PipelineRenderingCreateInfo = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR,
			.colorAttachmentCount = 1,
			.pColorAttachmentFormats = &colorFormat,
		};
		initInfo.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
		initInfo.CheckVkResultFn = check_vk_result;
		ImGui_ImplVulkan_Init(&initInfo);

		// Load Fonts
		// - If fonts are not explicitly loaded, Dear ImGui will select an embedded font: either AddFontDefaultVector() or AddFontDefaultBitmap().
		//   This selection is based on (style.FontSizeBase * style.FontScaleMain * style.FontScaleDpi) reaching a small threshold.
		// - You can load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
		// - If a file cannot be loaded, AddFont functions will return a nullptr. Please handle those errors in your code (e.g. use an assertion, display an error and quit).
		// - Read 'docs/FONTS.md' for more instructions and details.
		// - Use '#define IMGUI_ENABLE_FREETYPE' in your imconfig file to use FreeType for higher quality font rendering.
		// - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
		//style.FontSizeBase = 20.0f;
		//io.Fonts->AddFontDefaultVector();
		//io.Fonts->AddFontDefaultBitmap();
		//io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\segoeui.ttf");
		//io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf");
		//io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf");
		//io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf");
		//ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf");
		//IM_ASSERT(font != nullptr);
	}
}
