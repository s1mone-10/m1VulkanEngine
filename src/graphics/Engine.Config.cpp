#include "Engine.hpp"

#include <ranges>

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

	void Engine::setParticlesEnabled(bool enabled)
	{
		_config.particlesEnabled = enabled;
	}

	bool Engine::getParticlesEnabled() const { return _config.particlesEnabled;}

	void Engine::setShadowsEnabled(bool enabled)
	{
		_config.shadowsEnabled = enabled;
	}

	bool Engine::getShadowsEnabled() const { return _config.shadowsEnabled;}

	void Engine::setLightingType(LightingType lightingType)
	{
		if (_config.lightingType == lightingType) return;

		_config.lightingType = lightingType;

		if (_config.lightingType == LightingType::Pbr)
		{
			_defaultMaterial->syncPbrFromBlinnPhong();
			for (auto& material : _materials | std::views::values)
				material->syncPbrFromBlinnPhong();
		}
		else
		{
			_defaultMaterial->syncBlinnPhongFromPbr();
			for (auto& material : _materials | std::views::values)
				material->syncBlinnPhongFromPbr();
		}
	}

	LightingType Engine::getLightingType() const { return _config.lightingType;}

	void Engine::setUiEnabled(bool enabled)
	{
		_config.uiEnabled = enabled;
	}

	bool Engine::getUiEnabled() const { return _config.uiEnabled;}
}
