#include "log/Log.hpp"
#include "graphics/Engine.hpp"
#include "graphics/SceneObject.hpp"
#include "geometry/Vertex.hpp"
#include "geometry/Mesh.hpp"
#include "graphics/Material.hpp"

//libs
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

void loadScene(m1::Engine& engine);
void loadObj(m1::Engine& engine, const std::string &path);
void loadCubes(m1::Engine& engine, const uint32_t numCubes);

int main()
{
	m1::Log::Get().SetLevel(m1::LogLevel::Warning);
    m1::Log::Get().Info("Application starting");

	m1::EngineConfig engineConfig
	{
		.msaa = true,
		.shadows = true,
	};
    m1::Engine engine{engineConfig};

    try
    {
        loadScene(engine);
    	engine.compile();
        engine.run();
    }
    catch (const std::exception &e)
    {
        m1::Log::Get().Error(e.what());
        return EXIT_FAILURE;
    }

    m1::Log::Get().Info("Application finished successfully");
    return EXIT_SUCCESS;
}

void loadScene(m1::Engine& engine)
{
    const std::string MODEL_PATH = "../resources/viking_room.obj";
    //const std::string MODEL_PATH = "../resources/colored_cube.obj";
    //const std::string MODEL_PATH = "../resources/smooth_vase.obj";
    //const std::string MODEL_PATH = "../resources/flat_vase.obj";
    const std::string TEXTURE_PATH = "../resources/viking_room.png";


    loadCubes(engine, 3);
    //loadObj(engine, MODEL_PATH);
}

void loadObj(m1::Engine& engine, const std::string& path)
{
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, path.c_str()))
    {
        throw std::runtime_error(warn + err);
    }

    std::unordered_map<m1::Vertex, uint32_t> uniqueVertices{};
    auto sceneObj = m1::SceneObject::createSceneObject();

    for (const auto& shape : shapes)
    {
        for (const auto& index : shape.mesh.indices)
        {
            m1::Vertex vertex{};

            if (index.vertex_index >= 0)
            {
                vertex.pos = {
                    attrib.vertices[3 * index.vertex_index + 0],
                    attrib.vertices[3 * index.vertex_index + 1],
                    attrib.vertices[3 * index.vertex_index + 2],
                };

                vertex.color = {
                    attrib.colors[3 * index.vertex_index + 0],
                    attrib.colors[3 * index.vertex_index + 1],
                    attrib.colors[3 * index.vertex_index + 2],
                };
            }

            if (index.normal_index >= 0)
            {
                vertex.normal = {
                    attrib.normals[3 * index.normal_index + 0],
                    attrib.normals[3 * index.normal_index + 1],
                    attrib.normals[3 * index.normal_index + 2],
                };
            }

            if (index.texcoord_index >= 0)
            {
                vertex.texCoord = {
                    attrib.texcoords[2 * index.texcoord_index + 0],
                    1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
                };
            }

            if (uniqueVertices.count(vertex) == 0)
            {
                uniqueVertices[vertex] = static_cast<uint32_t>(sceneObj->Mesh->Vertices.size());
                sceneObj->Mesh->Vertices.push_back(vertex);
            }

            sceneObj->Mesh->Indices.push_back(uniqueVertices[vertex]);
        }
    }

    engine.addSceneObject(std::move(sceneObj));
}

void loadCubes(m1::Engine &engine, const uint32_t numCubes)
{
	// Initialize materials array with different material types

	// Shiny material (high specular, moderate diffuse)
	engine.addMaterial(std::make_unique<m1::Material>(
		"shiny",
		glm::vec3(0.7f, 0.0f, 0.0f),
		glm::vec3(0.5f, 0.5f, 0.5f),
		glm::vec3(0.7f, 0.0f, 0.0f),
		32.0f,
		1.0f
	));

	// Matte material (low specular, high diffuse)
	engine.addMaterial(std::make_unique<m1::Material>(
		"matte",
		glm::vec3(0.8f, 0.8f, 0.8f),
		glm::vec3(0.1f, 0.1f, 0.1f),
		glm::vec3(0.1f, 0.1f, 0.1f),
		1.0f,
		1.0f
	));

	// Emissive material (very high specular and diffuse for glow effect)
	engine.addMaterial(std::make_unique<m1::Material>(
		"emissive",
		glm::vec3(5.0f, 5.0f, 5.0f),
		glm::vec3(5.0f, 5.0f, 5.0f),
		glm::vec3(1.0f, 1.0f, 1.0f),
		64.0f,
		1.0f
	));

	// container texture
	glm::vec3 white(1.0f, 1.0f, 1.0f);
	auto material = std::make_unique<m1::Material>("container", white,white,white);
	material->diffuseTexturePath = "../resources/container.png";
	material->specularTexturePath = "../resources/container_specular.png";
	engine.addMaterial(std::move(material));

	// floor
	auto sceneObj = m1::SceneObject::createSceneObject();
	auto mesh = m1::Mesh::createQuad({0.5f, 0.5f, 0.5f});
	sceneObj->setMesh(std::move(mesh));
	engine.addSceneObject(std::move(sceneObj));

	// cube that represents the light source
    sceneObj = m1::SceneObject::createSceneObject();
	sceneObj->IsAuxiliary = true;
	mesh = m1::Mesh::createCube(glm::vec3(1.0f, 1.0f, 1.0f));
    sceneObj->setMesh(std::move(mesh));
    auto transform = glm::translate(glm::mat4(1.0f), glm::vec3(5.2f, 5.2f, 6.2f));
    transform = glm::scale(transform, glm::vec3(.1f));
    sceneObj->setTransform(transform);
	sceneObj->PipelineKey = m1::PipelineType::NoLight;
    engine.addSceneObject(std::move(sceneObj));

	bool random = false;
	if (random)
	{
		glm::vec3 cubePositions[] = {
			glm::vec3( 0.0f,  0.0f,  0.0f),
			glm::vec3( 2.0f,  5.0f, -15.0f),
			glm::vec3(-1.5f, -2.2f, -2.5f),
			glm::vec3(-3.8f, -2.0f, -12.3f),
			glm::vec3( 2.4f, -0.4f, -3.5f),
			glm::vec3(-1.7f,  3.0f, -7.5f),
			glm::vec3( 1.3f, -2.0f, -2.5f),
			glm::vec3( 1.5f,  2.0f, -2.5f),
			glm::vec3( 1.5f,  0.2f, -1.5f),
			glm::vec3(-1.3f,  1.0f, -1.5f)
		};

		for(unsigned int i = 0; i < 10; i++)
		{
			sceneObj = m1::SceneObject::createSceneObject();
			mesh = m1::Mesh::createCube();
			mesh->setMaterialName("container");
			sceneObj->setMesh(std::move(mesh));

			transform = glm::translate(glm::mat4(1.0f), cubePositions[i]);
			float angle = 20.0f * i;
			transform = glm::rotate(transform, glm::radians(angle), glm::vec3(1.0f, 0.3f, 0.5f));
			sceneObj->setTransform(transform);
			engine.addSceneObject(std::move(sceneObj));
		}
	}
	else
	{
		// cube grid
		for (uint32_t i = 0; i < numCubes; i++)
		{
			for (uint32_t j = 0; j < numCubes; j++)
			{
				for (uint32_t k = 0; k < numCubes; k++)
				{
					sceneObj = m1::SceneObject::createSceneObject();
					mesh = m1::Mesh::createCube();
					mesh->setMaterialName("container");
					sceneObj->setMesh(std::move(mesh));
					transform = glm::translate(glm::mat4(1.0f), glm::vec3(i* 2, j * 2, k * 2));
					sceneObj->setTransform(transform);
					engine.addSceneObject(std::move(sceneObj));
				}
			}
		}
	}
}

