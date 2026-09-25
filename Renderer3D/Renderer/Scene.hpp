#pragma once
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include "Renderer.hpp"
#include <Eigen/Dense>

struct Vec2
{
	float x, y;
};

struct Vec3
{
	float x, y, z;
};

struct RenderItem
{
	std::vector<size_t> meshIdx;
	std::string name;
	Eigen::Matrix4f transformation;
	Eigen::Vector4i index;
	uint32_t uboOffset;
};

struct MeshCollection
{
	std::vector<uint32_t> vbOffset;
	std::vector<uint32_t> ibOffset;
	std::vector<uint32_t> indexCount;
	std::vector<uint32_t> colorTexIndex;
};

struct RenderingData
{
	const std::vector<RenderItem>& renderItems;
	const MeshCollection& sceneGeometry;

};

struct TextureDesc
{
	int height;
	int width;
	int components; // if comp == 4 -> RGBA
};
struct Material
{
	std::string name;
	std::string baseColorPath;
	TextureDesc colorTex;
	std::string normalsPath;
	uint32_t colorIndex; // if index == UIN32_MAX then material has no color 
};

class Scene
{
public:
	Scene(
		const std::string& rootPath,
		const std::string& sceneName);

	void parseObjectTree(
		aiNode* node,
		const Eigen::Matrix4f& transform);

	void UploadTextureData(
		Renderer* renderer,
		Image* imageResource);

	void UploadObjectTransforms(char* mmap);

	RenderingData GetRenderingData() { return{ renderItems, sceneGeometry }; }

	inline size_t GetRenderItemCount() { return renderItems.size(); }

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

	inline uint32_t GetColorMaterialCount() { return colorMaterials; }

	inline TextureDesc GetColorTextureDesc() { return materials[0].colorTex; }
private:
	std::string rootPath;
	std::string sceneName;
	MeshCollection sceneGeometry;
	size_t vertexCount = 0;
	size_t indexCount = 0;
	std::vector<Vec3> vertices;
	std::vector<Vec3> normals;
	std::vector<Vec2> texCoords;
	std::vector <uint32_t> indecies;
	std::vector<Material> materials;
	std::vector<RenderItem> renderItems;
	uint32_t uboOffset;
	uint32_t colorMaterials;

};

