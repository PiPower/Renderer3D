#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <string>
#include <unordered_map>
#include "Resource.hpp"

class RenderGraph;

struct RenderPass
{
	RenderPass(RenderGraph* rg, bool isGraphicsPass) : rg(rg), isGraphicsPass(isGraphicsPass){};

	void AddTextureImage(const std::string& name);

	void AddInputImage(const std::string& name);

	void AddOutputImage(const std::string& name);

	void AddUniformBuffer(const std::string& name);

	void AddVertexBuffer(const std::string& name);

	void AddIndexBuffer(const std::string& name);

	void AddVertexShader(const std::string& name);

	void AddFragmentShader(const std::string& name);

private:
	RenderGraph* rg;
	bool isGraphicsPass;
	std::vector<const BufferResource*> vertexBuffers;
	std::vector<const BufferResource*> indexBuffers;
	std::vector<const BufferResource*> buffers;

	std::vector<const ImageResource*> inputImages;
	std::vector<const ImageResource*> outputImages;
	std::vector<const ImageResource*> textureImages;

	std::string shVertexName;
	std::string shFragmentName;
};
