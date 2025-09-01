#include "Vertex.hpp"
#include "Mesh.hpp"
#include "../log/Log.hpp"

namespace m1
{
	Mesh::Mesh()
	{
		Log::Get().Info("Creating mesh");
	}

	Mesh::~Mesh()
	{
		Log::Get().Info("Destroying mesh");
	}

}