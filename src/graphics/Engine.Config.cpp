#include "Engine.hpp"

#include <ranges>
#include <algorithm>

namespace m1
{
	void Engine::setMsaaEnabled(bool enabled)
	{
		if (_config.msaaEnabled == enabled) return;

		_config.msaaEnabled = enabled;
		vkDeviceWaitIdle(_device.getVkDevice());
		recreateSwapChain();
		createPipelines();
	}

	bool Engine::getMsaaEnabled() const { return _config.msaaEnabled;}

	void Engine::setParticlesEnabled(bool enabled) { _config.particlesEnabled = enabled; }

	bool Engine::getParticlesEnabled() const { return _config.particlesEnabled;}

	void Engine::setShadowsEnabled(bool enabled) { _config.shadowsEnabled = enabled; }

	bool Engine::getShadowsEnabled() const { return _config.shadowsEnabled;}

	void Engine::setLightingType(LightingType lightingType)	{ _config.lightingType = lightingType; }

	LightingType Engine::getLightingType() const { return _config.lightingType;}

	void Engine::setSkyboxEnabled(bool enabled) { _config.skyboxEnabled = enabled; }

	bool Engine::getSkyboxEnabled() const { return _config.skyboxEnabled; }

	void Engine::setEnvironmentMapIntensity(float intensity) { _config.environmentMapIntensity = std::max(0.0f, intensity); }

	float Engine::getEnvironmentMapIntensity() const { return _config.environmentMapIntensity; }

	void Engine::setEnvironmentMapPreset(EnvironmentMapPreset preset) { _config.environmentMapPreset = preset; }

	EnvironmentMapPreset Engine::getEnvironmentMapPreset() const { return _config.environmentMapPreset; }

	void Engine::setSelectedModelIndex(int modelIndex) { _config.selectedModelIndex = std::max(0, modelIndex); }

	int Engine::getSelectedModelIndex() const { return _config.selectedModelIndex; }

	void Engine::setAmbientLight(const glm::vec4& ambient) { _lightsUbo.ambient = ambient; }

	glm::vec4 Engine::getAmbientLight() const { return _lightsUbo.ambient; }

	void Engine::setLight(uint32_t index, const Light& light)
	{
		if (index >= MAX_LIGHTS)
			return;

		_lightsUbo.lights[index] = light;
	}

	Light Engine::getLight(uint32_t index) const
	{
		if (index >= MAX_LIGHTS)
			return {};

		return _lightsUbo.lights[index];
	}

	void Engine::setLightsCount(int lightsCount)
	{
		_lightsUbo.numLights = std::clamp(lightsCount, 0, MAX_LIGHTS);
	}

	int Engine::getLightsCount() const
	{
		return std::clamp(_lightsUbo.numLights, 0, MAX_LIGHTS);
	}

	void Engine::setUiEnabled(bool enabled) { _config.uiEnabled = enabled; }

	bool Engine::getUiEnabled() const { return _config.uiEnabled; }
}
