#pragma once

#include "Vertex.hpp"
#include <vector>

namespace m1 
{

	struct Mesh 
	{
	public:
		Mesh();
		~Mesh();

		std::vector<Vertex> Vertices;
		std::vector<uint32_t> Indices;
	private:
		
	};
}