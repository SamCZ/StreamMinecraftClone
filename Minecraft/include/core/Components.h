#ifndef MINECRAFT_COMPONENTS_H
#define MINECRAFT_COMPONENTS_H
#include "core.h"

namespace Minecraft
{
	struct Transform
	{
		glm::vec3 position;
		glm::vec3 scale;
		glm::vec3 orientation;

		glm::vec3 forward;
		glm::vec3 up;
		glm::vec3 right;
	};
}

#endif 