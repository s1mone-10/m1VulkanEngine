#include <chrono>
#include <iostream>

#include <GltfReader.hpp>
#include <stb_image.h>

#include "graphics/Engine.hpp"
#include "graphics/Sampler.hpp"
#include "graphics/SceneObject.hpp"
#include "graphics/Material.hpp"
#include "graphics/Texture.hpp"
#include "graphics/Image.hpp"

namespace m1
{
	bool GltfReader::loadGltf(Engine &engine, const std::filesystem::path &path)
	{
		if (!std::filesystem::exists(path))
		{
			std::cout << "Failed to find " << path << '\n';
			return false;
		}

		if constexpr (std::is_same_v<std::filesystem::path::value_type, wchar_t>)
		{
			std::wcout << "Loading " << path << '\n';
		}
		else
		{
			std::cout << "Loading " << path << '\n';
		}

		// Parse the glTF file and get the constructed asset
		{
			static constexpr auto supportedExtensions =
					fastgltf::Extensions::KHR_mesh_quantization |
					fastgltf::Extensions::KHR_texture_transform |
					fastgltf::Extensions::KHR_materials_variants;

			fastgltf::Parser parser(supportedExtensions);

			constexpr auto gltfOptions =
					fastgltf::Options::DontRequireValidAssetMember |
					fastgltf::Options::AllowDouble |
					fastgltf::Options::LoadExternalBuffers |
					fastgltf::Options::LoadExternalImages |
					fastgltf::Options::GenerateMeshIndices;

			auto gltfFile = fastgltf::MappedGltfFile::FromPath(path);
			if (!static_cast<bool>(gltfFile))
			{
				std::cerr << "Failed to open glTF file: " << fastgltf::getErrorMessage(gltfFile.error()) << '\n';
				return false;
			}

			auto asset = parser.loadGltf(gltfFile.get(), path.parent_path(), gltfOptions);
			if (asset.error() != fastgltf::Error::None)
			{
				std::cerr << "Failed to load glTF: " << fastgltf::getErrorMessage(asset.error()) << '\n';
				return false;
			}

			_asset = std::move(asset.get());

			// load samplers
			loadSamplers(engine);

			// load materials and textures
			images.resize(_asset.images.size());
			textures.resize(_asset.textures.size());
			for (auto &material: _asset.materials)
				loadMaterial(material, engine);

			// load meshes
			for (auto &mesh: _asset.meshes)
				meshes.push_back(loadMesh(mesh));

			// load nodes
			for (auto& node : _asset.nodes)
				loadNode(node, engine);

			for (auto& mat: materials)
				engine.addMaterial(std::move(mat));
		}

		return true;
	}

	void GltfReader::loadSamplers(Engine& engine)
	{
		auto extract_filter = [&](fastgltf::Filter filter)
		{
			switch (filter)
			{
				// nearest samplers
				case fastgltf::Filter::Nearest:
				case fastgltf::Filter::NearestMipMapNearest:
				case fastgltf::Filter::NearestMipMapLinear:
					return VK_FILTER_NEAREST;

				// linear samplers
				case fastgltf::Filter::Linear:
				case fastgltf::Filter::LinearMipMapNearest:
				case fastgltf::Filter::LinearMipMapLinear:
				default:
					return VK_FILTER_LINEAR;
			}
		};

		auto extract_mipmap_mode = [&](fastgltf::Filter filter)
		{
			switch (filter)
			{
				case fastgltf::Filter::NearestMipMapNearest:
				case fastgltf::Filter::LinearMipMapNearest:
					return VK_SAMPLER_MIPMAP_MODE_NEAREST;

				case fastgltf::Filter::NearestMipMapLinear:
				case fastgltf::Filter::LinearMipMapLinear:
				default:
					return VK_SAMPLER_MIPMAP_MODE_LINEAR;
			}
		};

		for (fastgltf::Sampler& sampler: _asset.samplers)
		{
			VkSamplerCreateInfo samplerCreateInfo = {.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO, .pNext = nullptr};
			samplerCreateInfo.maxLod = VK_LOD_CLAMP_NONE;
			samplerCreateInfo.minLod = 0;

			samplerCreateInfo.magFilter = extract_filter(sampler.magFilter.value_or(fastgltf::Filter::Nearest));
			samplerCreateInfo.minFilter = extract_filter(sampler.minFilter.value_or(fastgltf::Filter::Nearest));

			samplerCreateInfo.mipmapMode = extract_mipmap_mode(sampler.minFilter.value_or(fastgltf::Filter::Nearest));

			samplers.push_back(std::make_shared<Sampler>(engine.getDevice(), &samplerCreateInfo));
		}
	}

	void GltfReader::loadNode(const fastgltf::Node& gltfNode, Engine& engine)
	{
		// get transformation
		auto matrix = fastgltf::getTransformMatrix(gltfNode);
		glm::mat4 transform = glm::mat4(1.0f);
		for (int column = 0; column < 4; ++column)
			for (int row = 0; row < 4; ++row)
				transform[column][row] = matrix[column][row];

		// assign mesh
		if (gltfNode.meshIndex.has_value())
		{
			auto nodeMeshes = meshes[gltfNode.meshIndex.value()];

			for (auto& m: nodeMeshes)
			{
				auto sceneObj = SceneObject::createSceneObject();
				sceneObj->setMesh(m);
				sceneObj->setTransform(transform);
				engine.addSceneObject(std::move(sceneObj));
			}
		}

		// load children
		if (gltfNode.children.size() > 0)
		{
			for (const auto& childIndex: gltfNode.children)
			{
				loadNode(_asset.nodes[childIndex], engine);
			}
		}
	}

	std::vector<std::shared_ptr<Mesh>> GltfReader::loadMesh(const fastgltf::Mesh& gltfMesh)
	{
		std::vector<std::shared_ptr<Mesh>> primitives;

		for (const auto& gltfPrimitive: gltfMesh.primitives)
		{
			if (gltfPrimitive.type != fastgltf::PrimitiveType::Triangles)
				continue;

			auto mesh = std::make_unique<Mesh>();

			// Position
			auto position = gltfPrimitive.findAttribute("POSITION");
			assert(position != gltfPrimitive.attributes.end());
			// A mesh primitive is required to hold the POSITION attribute.
			assert(gltfPrimitive.indicesAccessor.has_value()); // we should always have indices

			auto& positionAccessor = _asset.accessors[position->accessorIndex];
			if (!positionAccessor.bufferViewIndex.has_value())
				continue;
			std::vector<Vertex> vertices(positionAccessor.count);

			fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(_asset, positionAccessor,
				[&](fastgltf::math::fvec3 pos, std::size_t idx)
				{
					vertices[idx].pos = glm::vec3(
						pos.x(), pos.y(), pos.z());
				});

			// Material
			std::size_t baseColorTexcoordIndex = 0;
			if (gltfPrimitive.materialIndex.has_value())
			{
				auto matIndex = gltfPrimitive.materialIndex.value();
				mesh->setMaterialName(materials[matIndex]->name);
				auto& gltfMaterial = _asset.materials[matIndex];

				auto& baseColorTexture = gltfMaterial.pbrData.baseColorTexture;
				if (baseColorTexture.has_value())
				{
					auto& texture = _asset.textures[baseColorTexture->textureIndex];
					if (!texture.imageIndex.has_value())
						continue;

					if (baseColorTexture->transform && baseColorTexture->transform->texCoordIndex.has_value())
						baseColorTexcoordIndex = baseColorTexture->transform->texCoordIndex.value();
					else
						baseColorTexcoordIndex = gltfMaterial.pbrData.baseColorTexture->texCoordIndex;
				}
			}

			// TexCoord
			auto texcoordAttribute = std::string("TEXCOORD_") + std::to_string(baseColorTexcoordIndex);
			auto texCoord = gltfPrimitive.findAttribute(texcoordAttribute);
			if (texCoord != gltfPrimitive.attributes.end())
			{
				auto& texCoordAccessor = _asset.accessors[texCoord->accessorIndex];

				fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec2>(_asset, texCoordAccessor,
					[&](fastgltf::math::fvec2 uv, std::size_t idx)
					{
						vertices[idx].texCoord = glm::vec2(uv.x(), uv.y());
					});

			}

			// Normal
			auto normal = gltfPrimitive.findAttribute("NORMAL");
			if (normal != gltfPrimitive.attributes.end())
			{
				const auto& normalAccessor = _asset.accessors[normal->accessorIndex];
				fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(_asset, normalAccessor,
					[&](fastgltf::math::fvec3 nor, std::size_t idx)
					{
						vertices[idx].normal = glm::vec3(
							nor.x(), nor.y(), nor.z());
					});
			}

			// Tangents
			auto tangent = gltfPrimitive.findAttribute("TANGENT");
			if (tangent != gltfPrimitive.attributes.end())
			{
				const auto& tangentAccessor = _asset.accessors[tangent->accessorIndex];
				fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec4>(_asset, tangentAccessor,
					[&](fastgltf::math::fvec4 tan, std::size_t idx)
					{
						vertices[idx].tangent = glm::vec4(tan.x(), tan.y(), tan.z(), tan.w());
					});
			}

			// vertex colors
			auto color = gltfPrimitive.findAttribute("COLOR_0");
			if (color != gltfPrimitive.attributes.end())
			{
				const auto& colorAccessor = _asset.accessors[color->accessorIndex];
				fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec4>(_asset, colorAccessor,
					[&](fastgltf::math::fvec4 col, std::size_t idx)
					{
						vertices[idx].color = glm::vec3(col.x(), col.y(), col.z());
					});
			}

			// Indices
			auto& indexAccessor = _asset.accessors[gltfPrimitive.indicesAccessor.value()];
			if (!indexAccessor.bufferViewIndex.has_value())
				continue;

			std::vector<uint32_t> indices(indexAccessor.count);
			fastgltf::iterateAccessorWithIndex<std::uint32_t>(_asset, indexAccessor,
				[&](std::uint32_t index, std::size_t idx)
				{
					indices[idx] = index;
				});

			mesh->Vertices = std::move(vertices);
			mesh->Indices = std::move(indices);

			primitives.push_back(std::move(mesh));
		}

		return primitives;
	}

	std::shared_ptr<Image> GltfReader::loadImage(fastgltf::Image& image, Engine& engine, VkFormat format)
	{
		std::shared_ptr<Image> myImage;

		auto createImage = [&](unsigned char* data, int width, int height)
		{
			uint32_t w = static_cast<uint32_t>(width);
			uint32_t h = static_cast<uint32_t>(height);
			ImageParams params
			{
				.extent = {w, h},
				.format = format,
				.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
				.mipLevels = Texture::computeMipLevels(w, h)
			};
			myImage = std::move(engine.createImage(params, data));
		};

		// TODO use KTX2 library? it should also contains mipLevels and image format

		std::visit(fastgltf::visitor{
			           [](auto &arg) {},
			           [&](fastgltf::sources::URI &filePath)
			           {
				           assert(filePath.fileByteOffset == 0); // We don't support offsets with stbi.
				           assert(filePath.uri.isLocalPath()); // We're only capable of loading local files.
				           int width, height, nrChannels;

				           const std::string path(filePath.uri.path().begin(), filePath.uri.path().end());
				           // Thanks C++.
				           unsigned char *data = stbi_load(path.c_str(), &width, &height, &nrChannels, 4);

				           createImage(data, width, height);
				           stbi_image_free(data);
			           },
			           [&](fastgltf::sources::Array &vector)
			           {
				           int width, height, nrChannels;
				           unsigned char *data = stbi_load_from_memory(
					           reinterpret_cast<const stbi_uc *>(vector.bytes.data()),
					           static_cast<int>(vector.bytes.size()), &width, &height, &nrChannels, 4);

				           createImage(data, width, height);
				           stbi_image_free(data);
			           },
			           [&](fastgltf::sources::BufferView &view)
			           {
				           auto &bufferView = _asset.bufferViews[view.bufferViewIndex];
				           auto &buffer = _asset.buffers[bufferView.bufferIndex];
				           // Yes, we've already loaded every buffer into some GL buffer. However, with GL it's simpler
				           // to just copy the buffer data again for the texture. Besides, this is just an example.
				           std::visit(fastgltf::visitor{
					                      // We only care about VectorWithMime here, because we specify LoadExternalBuffers, meaning
					                      // all buffers are already loaded into a vector.
					                      [](auto &arg) {},
					                      [&](fastgltf::sources::Array &vector)
					                      {
						                      int width, height, nrChannels;
						                      unsigned char *data = stbi_load_from_memory(
							                      reinterpret_cast<const stbi_uc *>(
								                      vector.bytes.data() + bufferView.byteOffset),
							                      static_cast<int>(bufferView.byteLength), &width, &height, &nrChannels,
							                      4);

						                      createImage(data, width, height);
						                      stbi_image_free(data);
					                      }
				                      }, buffer.data);
			           },
		           }, image.data);

		return myImage;
	}

	std::shared_ptr<Texture> GltfReader::loadTexture(Engine& engine, const fastgltf::TextureInfo& textureInfo, VkFormat format)
	{
		// Check if the texture is already loaded
		if (textures[textureInfo.textureIndex] != nullptr)
			return textures[textureInfo.textureIndex];

		// get image and sampler indices
		auto texture = _asset.textures[textureInfo.textureIndex];
		size_t imgIndex = texture.imageIndex.value();
		size_t samplerIndex = texture.samplerIndex.value();

		// load the image if missing
		if (images[imgIndex] == nullptr)
			images[imgIndex] = loadImage(_asset.images[imgIndex], engine, format);

		// create the texture
		textures[textureInfo.textureIndex] = std::make_shared<Texture>(engine.getDevice(), images[imgIndex], samplers[samplerIndex]);
		return textures[textureInfo.textureIndex];
	}

	bool GltfReader::loadMaterial(fastgltf::Material& gltfMaterial, Engine& engine)
	{
		auto myMaterial = std::make_unique<Material>(gltfMaterial.name.c_str());

		auto& pbrData = gltfMaterial.pbrData;
		myMaterial->baseColor.r = pbrData.baseColorFactor[0];
		myMaterial->baseColor.g = pbrData.baseColorFactor[1];
		myMaterial->baseColor.b = pbrData.baseColorFactor[2];
		myMaterial->baseColor.a = pbrData.baseColorFactor[3];

		myMaterial->metallicFactor = pbrData.metallicFactor;
		myMaterial->roughnessFactor = pbrData.roughnessFactor;
		myMaterial->emissiveFactor = glm::vec3(gltfMaterial.emissiveFactor.x(), gltfMaterial.emissiveFactor.y(),
			gltfMaterial.emissiveFactor.z());

		// TODO
		// MaterialPass passType = MaterialPass::MainColor;
		// if (material.alphaMode == fastgltf::AlphaMode::Blend)
		// {
		// 	passType = MaterialPass::Transparent;
		// }

		// grab textures
		if (pbrData.baseColorTexture.has_value())
			myMaterial->baseColorMap = loadTexture(engine, pbrData.baseColorTexture.value(), VK_FORMAT_R8G8B8A8_SRGB);

		if (pbrData.metallicRoughnessTexture.has_value())
			myMaterial->metallicRoughnessMap = loadTexture(engine, pbrData.metallicRoughnessTexture.value(), VK_FORMAT_R8G8B8A8_UNORM);

		if (gltfMaterial.normalTexture.has_value())
			myMaterial->normalMap = loadTexture(engine, gltfMaterial.normalTexture.value(), VK_FORMAT_R8G8B8A8_UNORM);

		if (gltfMaterial.occlusionTexture.has_value())
			myMaterial->occlusionMap = loadTexture(engine, gltfMaterial.occlusionTexture.value(), VK_FORMAT_R8G8B8A8_UNORM);

		if (gltfMaterial.emissiveTexture.has_value())
			myMaterial->emissiveMap = loadTexture(engine, gltfMaterial.emissiveTexture.value(), VK_FORMAT_R8G8B8A8_SRGB);

		materials.push_back(std::move(myMaterial));

		return true;
	}
}
