#include "UiModule.hpp"
#include "Engine.hpp"
#include "Queue.hpp"
#include "Renderer.hpp"
#include "Utils.hpp"
#include <algorithm>

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
	UiModule::UiModule(Engine& engine, const Window& window, const SwapChain& swapChain)
		: _engine(engine)
	{
		createDescriptorPool();
		initImGui(window, swapChain);
	}

	UiModule::~UiModule()
	{
		ImGui_ImplVulkan_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();

		vkDestroyDescriptorPool(_engine.getDevice().getVkDevice(), _descriptorPool, nullptr);
	}

	void UiModule::build() const
	{
		// Start the Dear ImGui frame
		ImGui_ImplVulkan_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		constexpr float minPanelWidth = 360.0f;
		constexpr float preferredPanelWidth = 420.0f;
		ImGuiIO& io = ImGui::GetIO();
		float panelWidth = std::clamp(io.DisplaySize.x * 0.33f, minPanelWidth, preferredPanelWidth);
		panelWidth = std::min(panelWidth, io.DisplaySize.x);

		ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - panelWidth, 0.0f), ImGuiCond_Always);
		ImGui::SetNextWindowSize(ImVec2(panelWidth, io.DisplaySize.y), ImGuiCond_Always);
		ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse;
		ImGui::Begin("Engine controls", nullptr, windowFlags);
		ImGui::PushItemWidth(-1.0f);

		ImGui::TextUnformatted("Rendering");
		ImGui::Separator();

		bool enableMsaa = _engine.getMsaaEnabled();
		if (ImGui::Checkbox("MSAA", &enableMsaa))
			_engine.setMsaaEnabled(enableMsaa);

		bool particlesEnabled = _engine.getParticlesEnabled();
		if (ImGui::Checkbox("Particles", &particlesEnabled))
			_engine.setParticlesEnabled(particlesEnabled);

		bool shadowsEnabled = _engine.getShadowsEnabled();
		if (ImGui::Checkbox("Shadows", &shadowsEnabled))
			_engine.setShadowsEnabled(shadowsEnabled);

		bool skyboxEnabled = _engine.getSkyboxEnabled();
		if (ImGui::Checkbox("Skybox", &skyboxEnabled))
			_engine.setSkyboxEnabled(skyboxEnabled);

		ImGui::TextUnformatted("Skybox map");
		const char* skyBoxMapItems[] = {"Environment", "Irradiance", "Prefiltered"};
		int skyBoxMode= 0;
		switch (_engine.getSkyBoxMap())
		{
			case SkyBoxMap::Environment:
				skyBoxMode = 0;
				break;
			case SkyBoxMap::Irradiance:
				skyBoxMode = 1;
				break;
			case SkyBoxMap::PrefilteredEnv:
				skyBoxMode = 2;
				break;
		}
		if (ImGui::Combo("##Sky box map", &skyBoxMode, skyBoxMapItems, IM_ARRAYSIZE(skyBoxMapItems)))
		{
			switch (skyBoxMode)
			{
				case 0:
					_engine.setSkyBoxMap(SkyBoxMap::Environment);
					break;
				case 1:
					_engine.setSkyBoxMap(SkyBoxMap::Irradiance);
					break;
				case 2:
					_engine.setSkyBoxMap(SkyBoxMap::PrefilteredEnv);
					break;
				default: ;
			}
		}

		ImGui::Spacing();
		ImGui::Spacing();
		ImGui::TextUnformatted("Lighting");
		ImGui::Separator();

		int lightingMode = _engine.getLightingType() == LightingType::BlinnPhong ? 0 : 1;
		const char* lightingItems[] = {"Blinn-Phong", "PBR"};
		if (ImGui::Combo("##Lighting mode", &lightingMode, lightingItems, IM_ARRAYSIZE(lightingItems)))
			_engine.setLightingType(lightingMode == 0 ? LightingType::BlinnPhong : LightingType::Pbr);

		ImGui::TextUnformatted("IBL intensity");
		float envIntensity = _engine.getIblIntensity();
		if (ImGui::SliderFloat("##IBL intensity", &envIntensity, 0.0f, 3.0f, "%.2f"))
			_engine.setIblIntensity(envIntensity);

		ImGui::TextUnformatted("Environment map");
		int envMapPreset = _engine.getEnvironmentMapPreset() == EnvironmentMapPreset::NewportLoft ? 0 : 1;
		const char* envMapItems[] = {"newport_loft", "HDR_111_Parking_Lot_2_Ref"};
		if (ImGui::Combo("##Environment map", &envMapPreset, envMapItems, IM_ARRAYSIZE(envMapItems)))
		{
			_engine.setEnvironmentMapPreset(envMapPreset == 0
				? EnvironmentMapPreset::NewportLoft
				: EnvironmentMapPreset::Hdr111ParkingLot2Ref);
		}

		ImGui::Spacing();
		ImGui::Spacing();
		ImGui::TextUnformatted("Scene");
		ImGui::Separator();

		// Placeholder list: user will replace with desired model list.
		int selectedModelIndex = _engine.getSelectedModelIndex();
		const char* gltfModels[] = {"DamagedHelmet.glb", "Model_2.glb", "Model_3.glb"};
		if (ImGui::Combo("##Scene model", &selectedModelIndex, gltfModels, IM_ARRAYSIZE(gltfModels)))
		{
			_engine.setSelectedModelIndex(selectedModelIndex);
		}

		ImGui::Spacing();
		ImGui::TextUnformatted("Lights");
		ImGui::Separator();

		int lightsCount = _engine.getLightsCount();
		if (ImGui::SliderInt("Lights count", &lightsCount, 0, MAX_LIGHTS))
		{
			_engine.setLightsCount(lightsCount);
		}

		glm::vec4 ambient = _engine.getAmbientLight();
		float ambientColor[3] = {ambient.r, ambient.g, ambient.b};
		if (ImGui::ColorEdit3("Ambient color", ambientColor))
		{
			ambient.r = ambientColor[0];
			ambient.g = ambientColor[1];
			ambient.b = ambientColor[2];
			_engine.setAmbientLight(ambient);
		}
		if (ImGui::SliderFloat("Ambient intensity", &ambient.a, 0.0f, 2.0f, "%.2f"))
		{
			_engine.setAmbientLight(ambient);
		}

		for (int i = 0; i < std::min(lightsCount, 2); ++i)
		{
			Light light = _engine.getLight(static_cast<uint32_t>(i));
			if (ImGui::TreeNode(std::format("Light {}", i).c_str()))
			{
				bool directional = light.posDir.w == 0.0f;
				if (ImGui::Checkbox(std::format("Directional##{}", i).c_str(), &directional))
				{
					light.posDir.w = directional ? 0.0f : 1.0f;
					_engine.setLight(i, light);
				}

				if (ImGui::DragFloat3(std::format("Pos/Dir##{}", i).c_str(), &light.posDir.x, 0.05f))
				{
					_engine.setLight(i, light);
				}

				float lightColor[3] = {light.color.r, light.color.g, light.color.b};
				if (ImGui::ColorEdit3(std::format("Color##{}", i).c_str(), lightColor))
				{
					light.color.r = lightColor[0];
					light.color.g = lightColor[1];
					light.color.b = lightColor[2];
					_engine.setLight(i, light);
				}
				if (ImGui::SliderFloat(std::format("Intensity##{}", i).c_str(), &light.color.a, 0.0f, 10.0f, "%.2f"))
				{
					_engine.setLight(i, light);
				}

				if (ImGui::DragFloat3(std::format("Attenuation##{}", i).c_str(), &light.attenuation.x, 0.001f, 0.0f, 2.0f, "%.3f"))
				{
					_engine.setLight(i, light);
				}
				ImGui::TreePop();
			}
		}

		ImGui::Spacing();
		ImGui::TextUnformatted("Note: UI wiring only; engine-side feature logic is still to be implemented.");
		ImGui::PopItemWidth();
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
		VK_CHECK(vkCreateDescriptorPool(_engine.getDevice().getVkDevice(), &pool_info, nullptr, &_descriptorPool));
	}

	void UiModule::initImGui(const Window& window, const SwapChain& swapChain)
	{
		auto& device = _engine.getDevice();

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
		initInfo.Instance = device.getVkInstance();
		initInfo.PhysicalDevice = device.getVkPhysicalDevice();
		initInfo.Device = device.getVkDevice();
		initInfo.QueueFamily = device.getQueueFamilyIndices().graphicsFamily.value();
		initInfo.Queue = device.getGraphicsQueue().getVkQueue();
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
