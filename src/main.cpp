#include "log/Log.hpp"
#include "graphics/Engine.hpp"
#include "graphics/SceneObject.hpp"
#include "geometry/Vertex.hpp"
#include "geometry/Mesh.hpp"
#include "geometry/Material.hpp"

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
    m1::Log::Get().Info("Application starting");
    m1::Engine app;

    try
    {
        loadScene(app);
    	app.compile();
        app.run();
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


    loadCubes(engine, 1);
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
		glm::vec3(1.0f, 1.0f, 1.0f),
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

    auto sceneObj = m1::SceneObject::createSceneObject();
	auto mesh = m1::Mesh::createCube();
	mesh->setMaterialName("shiny");
    sceneObj->setMesh(std::move(mesh));
    engine.addSceneObject(std::move(sceneObj));

    sceneObj = m1::SceneObject::createSceneObject();
	mesh = m1::Mesh::createCube();
	mesh->setMaterialName("emissive");
    sceneObj->setMesh(std::move(mesh));
    auto transform = glm::translate(glm::mat4(1.0f), glm::vec3(1.5f, 0.5f, 2.0f));
    transform = glm::scale(transform, glm::vec3(.2f));
    sceneObj->setTransform(transform);
    engine.addSceneObject(std::move(sceneObj));
}

