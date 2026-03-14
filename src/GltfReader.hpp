#pragma once

#include <fastgltf/core.hpp>
#include <fastgltf/types.hpp>
#include <fastgltf/tools.hpp>

#include "Mesh.hpp"
#include "graphics/Material.hpp"

namespace  m1
{
	class SceneObject;
	class Engine;
	class Sampler;
	class Image;

	class GltfReader
	{
	public:
		bool loadGltf(Engine& engine, const std::filesystem::path &path);

	private:
		fastgltf::Asset _asset;
		std::vector<std::vector<std::shared_ptr<Mesh>>> meshes;
		std::vector<std::unique_ptr<Material>> materials;
		std::vector<std::shared_ptr<Image>> images;
		std::vector<std::shared_ptr<Texture>> textures;
		std::vector<std::shared_ptr<Sampler>> samplers;

		void loadSamplers(Engine& engine);
		void loadNode(const fastgltf::Node& gltfNode, Engine& engine);
		std::vector<std::shared_ptr<Mesh>> loadMesh(const fastgltf::Mesh& gltfMesh);
		std::shared_ptr<Image> loadImage(fastgltf::Image& image, Engine& engine, VkFormat format);
		std::shared_ptr<Texture> loadTexture(Engine& engine, const fastgltf::TextureInfo& textureIndex, VkFormat format);
		bool loadMaterial(fastgltf::Material& gltfMaterial, Engine& engine);
	};
}
