#pragma once

#include "Vertex.hpp"
#include <vector>

namespace m1 
{

	class Mesh 
	{
	public:
		Mesh();
		~Mesh();

		const std::vector<Vertex> Vertices =
		{
			Vertex{{-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
			Vertex{{0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
			Vertex{{0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},
			Vertex{{-0.5f, 0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}}
		};

		const std::vector<uint16_t> Indices = {
			0, 1, 2, 2, 3, 0
		};
	private:
		
	};
}

