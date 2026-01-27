#pragma once

#include "geometry/Mesh.hpp"

// libs
#include "glm_config.hpp"

// std
#include <memory>

namespace m1
{
	class SceneObject
	{
	public:
		static std::unique_ptr<SceneObject> createSceneObject()
		{
			static uint64_t currentId = 0;
			// ReSharper disable once CppDFAMemoryLeak (it's not a leak)
			return std::unique_ptr<SceneObject>(new SceneObject(currentId++));
		}

		void setMesh(std::unique_ptr<Mesh> mesh) { Mesh = std::move(mesh); }
		void setTransform(const glm::mat4& transform) { Transform = transform; }

		uint64_t Id;
		glm::mat4 Transform{ 1.0f };
		std::unique_ptr<Mesh> Mesh = nullptr;
		// Optional: which pipeline to use when drawing this object
		std::optional<PipelineType> PipelineKey = std::nullopt;

	private:
		explicit SceneObject(const uint64_t id) : Id{ id } { }
	};
}