#pragma once
#include "../util/math.hpp"

namespace rv
{
	class texture;
	class font;

	struct color
	{
		float r, g, b, a;
	};

	struct position : vector_2d<float>
	{

	};

	struct ndc_position : vector_2d<float>
	{

	};

	struct texture_position : vector_2d<float>
	{

	};
}
