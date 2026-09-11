#include "RenderGraph.hpp"
#include <Windows.h>
#include <stdexcept>

RenderGraph::RenderGraph(
	const std::vector<std::string>& bufferNames,
	const std::vector<std::string>& imageNames)
{

	for (size_t i = 0; i < bufferNames.size(); ++i)
	{
		if (resourceBind.find(bufferNames[i]) != resourceBind.end())
		{
			throw std::runtime_error("Duplicate buffer name: " + bufferNames[i]);
		}

		resourceBind[bufferNames[i]] = buffResource.size();
		buffResource.push_back({});
	}

	for (size_t i = 0; i < imageNames.size(); ++i)
	{
		if (resourceBind.find(imageNames[i]) != resourceBind.end())
		{
			throw std::runtime_error("Duplicate image name: " + imageNames[i]);
		}
		resourceBind[imageNames[i]] = imgResource.size();
		imgResource.push_back({});
	}
}

RenderPass* RenderGraph::CreateRenderPass(
	const std::string& name,
	bool isGraphicsPass)
{
	if (renderPassNames.find(name) != renderPassNames.end())
	{
		throw std::runtime_error("RenderPass with name '" + name + "' already exists.");
	}

	renderPasses.push_back(RenderPass(this, isGraphicsPass));
	renderPassNames[name] = renderPasses.size() - 1;

	return &renderPasses.back();
}

void RenderGraph::AddShader(
	const std::string& name,
	const std::string& path,
	const std::string& entryName,
	VkShaderStageFlagBits shaderStage)
{
}

ImageResource* RenderGraph::QueryImage(const std::string& name)
{
	return nullptr;
}

BufferResource* RenderGraph::QueryBuffer(const std::string& name)
{
	return nullptr;
}
