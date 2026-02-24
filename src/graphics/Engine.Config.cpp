#include "Engine.hpp"

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

	void Engine::setUiEnabled(bool enabled)
	{
		_config.uiEnabled = enabled;
	}

	bool Engine::getUiEnabled() const { return _config.uiEnabled;}
}
