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

	inline uint64_t GetVertexCount() { return vertexCount; }

	inline uint64_t GetVertexByteSize() { return vertexCount * sizeof(Vec3); }

	inline uint64_t GetNormalsByteSize() { return vertexCount * sizeof(Vec3); }

	inline uint64_t GetTexByteSize() { return vertexCount * sizeof(Vec2); }

	inline uint64_t GetIndexCount() { return indexCount; }

	inline uint64_t GetIndexByteSize() { return indexCount * sizeof(uint32_t); }

	inline Vec3* GetVertexPtr() { return vertices.data(); }

	inline Vec3* GetNormalsPtr() { return normals.data(); }

	inline Vec2* GetTexPtr() { return texCoords.data(); }

	inline uint32_t* GetIndexPtr() { return indecies.data(); }
private:
	size_t vertexCount = 0;
	size_t indexCount = 0;
	std::vector<Vec3> vertices;
	std::vector<Vec3> normals;
	std::vector<Vec2> texCoords;
	std::vector <uint32_t> indecies;
	std::vector<uint32_t> materialTextureIdx;
};

