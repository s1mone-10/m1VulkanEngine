#pragma once

#include "Vertex.hpp"
#include "graphics/Component.hpp"
#include "graphics/Pipeline.hpp"

//libs
#include "graphics/glm_config.hpp"

//std
#include <memory>
#include <string>
#include <vector>

namespace m1
{
	class Buffer;
	class Device;

	// TODO create a MeshComponent class and keep Mesh only geometric? Reevaluate after having studied instancing.
	class Mesh final : public Component
	{
	private:
		void createVertexBuffer(const Device& device);
		void createIndexBuffer(const Device& device);

		std::unique_ptr<Buffer> _vertexBuffer;
		std::unique_ptr<Buffer> _indexBuffer;
		std::string _materialName;

	public:
		Mesh();

		[[nodiscard]] size_t GetTypeID() const override { return Component::GetTypeID<Mesh>(); }

		static std::unique_ptr<Mesh> createCube(const glm::vec3& color = glm::vec3(1.0f, 1.0f, 1.0f));
		static std::unique_ptr<Mesh> createQuad(const glm::vec3& color = glm::vec3(1.0f, 1.0f, 1.0f));

		void setMaterialName(const std::string& materialName) { _materialName = materialName; }
		[[nodiscard]] const std::string& getMaterialName() const { return _materialName; }

		void compile(const Device& device);
		void draw(VkCommandBuffer commandBuffer) const;

		std::vector<Vertex> Vertices;
		std::vector<uint32_t> Indices;
		std::optional<PipelineType> PipelineKey = std::nullopt;
	};
}
