#pragma once

// libs
#include "glm_config.hpp"

// std
#include <memory>
#include <unordered_map>

namespace m1
{
	class Material; // forward declaration
	class Mesh;

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
		int32_t MaterialUboIndex = -1;
		// Optional: which pipeline to use when drawing this object
		std::optional<PipelineName> PipelineKey = std::nullopt;

	private:
		explicit SceneObject(const uint64_t id) : Id{ id } { }
	};
}