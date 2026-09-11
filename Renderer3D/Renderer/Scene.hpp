#pragma once
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

struct Vec2
{
	float x, y;
};

struct Vec3
{
	float x, y, z;
};


class Scene
{
public:
	Scene(std::string path);

private:
	size_t vertexCount = 0;
	size_t indexCount = 0;
	Vec3* vertices;
	Vec3* normals;
	Vec3* texCoords;
	uint32_t* indecies;
	std::vector<uint32_t> materialTextureIdx;
};

