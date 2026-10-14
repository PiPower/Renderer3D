#pragma once
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include "Renderer.hpp"
#include <Eigen/Dense>
#include <vector>
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
	std::vector<size_t> materialIdx;
};

struct TextureDesc
{
	int height;
	int width;
	int components; // if comp == 4 -> RGBA
};

struct Material
{
	enum PathType
	{
		BaseColor,
		NormalMap
	};

	std::string name;
	std::array<std::string, 2> imagePaths;
	TextureDesc colorTex;
	TextureDesc normalMap;
	uint32_t colorIndex; // if colorIndex == UIN32_MAX then material has no color 
	uint32_t normalMapIndex; // if normalMapIndex == UIN32_MAX then material has no normal map
	float opacity;
};

struct RenderingData
{
	const std::vector<RenderItem>& renderItems;
	const MeshCollection& sceneGeometry;
	const std::vector<Material>& materials;
	size_t opaqueMaterialsCount;
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
		Image* imageResource,
		Material::PathType materialType);

	void UploadObjectTransforms(char* mmap);

	RenderingData GetRenderingData() { return{ renderItems, sceneGeometry, materials, opaqueMaterialsCount }; }

	inline size_t GetRenderItemCount() { return renderItems.size(); }

	inline uint64_t GetVertexCount() { return vertexCount; }

	inline uint64_t GetVertexByteSize() { return vertexCount * sizeof(Vec3); }

	inline uint64_t GetNormalsByteSize() { return vertexCount * sizeof(Vec3); }

	inline uint64_t GetTangentsByteSize() { return vertexCount * sizeof(Vec3); }

	inline uint64_t GetBiTangentsByteSize() { return vertexCount * sizeof(Vec3); }

	inline uint64_t GetTexByteSize() { return vertexCount * sizeof(Vec2); }

	inline uint64_t GetIndexCount() { return indexCount; }

	inline uint64_t GetIndexByteSize() { return indexCount * sizeof(uint32_t); }

	inline Vec3* GetVertexPtr() { return vertices.data(); }

	inline Vec3* GetTangentsPtr() { return tangents.data(); }
	
	inline Vec3* GetBiTangentsPtr() { return biTangents.data(); }

	inline Vec3* GetNormalsPtr() { return normals.data(); }

	inline Vec2* GetTexPtr() { return texCoords.data(); }

	inline uint32_t* GetIndexPtr() { return indecies.data(); }

	inline uint32_t GetColorMaterialCount() { return colorMaterials; }

	inline uint32_t GetNormalMapCount() { return normalMaps; }

	inline TextureDesc GetColorTextureDesc() { return materials[0].colorTex; }

	inline TextureDesc GetNormalMapDesc() { return materials[0].normalMap; }
private:
	std::string rootPath;
	std::string sceneName;
	MeshCollection sceneGeometry;
	size_t vertexCount = 0;
	size_t indexCount = 0;
	std::vector<Vec3> vertices;
	std::vector<Vec3> tangents;
	std::vector<Vec3> biTangents;
	std::vector<Vec3> normals;
	std::vector<Vec2> texCoords;
	std::vector<uint32_t> indecies;
	std::vector<Material> materials;
	std::vector<RenderItem> renderItems;
	uint32_t uboOffset;
	uint32_t colorMaterials;
	uint32_t normalMaps;
	size_t opaqueMaterialsCount;
};

