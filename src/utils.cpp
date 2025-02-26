#include "utils.h"

#include <glm/glm.hpp>
#include <assimp/Importer.hpp>

template <>
glm::vec3 Cast(const aiVector3D& src)
{
    return glm::vec3{ src.x, src.y, src.z };
}
