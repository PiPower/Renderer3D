#pragma once
#define  _CRT_SECURE_NO_WARNINGS
#include <vulkan/vulkan.h>
#include <unordered_map>
#include "RenderPass.hpp"

class RenderGraph
{
public:
	RenderGraph(
		const std::vector<std::string>& bufferNames,
		const std::vector<std::string>& imageNames);

	RenderPass* CreateRenderPass(
		const std::string& name,
		bool isGraphicsPass);

	void AddShader(
		const std::string& name,
		const std::string& path,
		const std::string& entryName,
		VkShaderStageFlagBits shaderStage);

	ImageResource* QueryImage(const std::string& name);

	BufferResource* QueryBuffer(const std::string& name);

private:
	std::vector<RenderPass> renderPasses;
	std::vector<ImageResource> imgResource;
	std::vector<BufferResource> buffResource;

	std::unordered_map<std::string, size_t> renderPassNames;
	std::unordered_map<std::string, size_t> resourceBind;
};
