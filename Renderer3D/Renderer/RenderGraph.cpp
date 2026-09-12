#include "RenderGraph.hpp"
#include <Windows.h>
#include <stdexcept>

RenderGraph::RenderGraph(
	const std::vector<std::string>& bufferNames,
	const std::vector<std::string>& imageNames,
	const std::vector<ShaderDesc>& shaderDescs)
	:
	execGraph(nullptr)
{

	for (size_t i = 0; i < bufferNames.size(); ++i)
	{
		if (bufferBind.find(bufferNames[i]) != bufferBind.end())
		{
			throw std::runtime_error("Duplicate buffer name: " + bufferNames[i]);
		}

		bufferBind[bufferNames[i]] = buffResource.size();
		buffResource.push_back({});
	}

	for (size_t i = 0; i < imageNames.size(); ++i)
	{
		if (imageBind.find(imageNames[i]) != imageBind.end())
		{
			throw std::runtime_error("Duplicate image name: " + imageNames[i]);
		}
		imageBind[imageNames[i]] = imgResource.size();
		imgResource.push_back({});
	}

	for (size_t i = 0; i < shaderDescs.size(); ++i)
	{
		if (shaderBind.find(shaderDescs[i].name) != shaderBind.end())
		{
			throw std::runtime_error("Duplicate shader name: " + shaderDescs[i].name);
		}
		shaderBind[shaderDescs[i].name] = shaders.size();
		shaders.push_back(shaderDescs[i]);
		shaders.back().stages = (VkShaderStageFlagBits)0;
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

void RenderGraph::Compile(Renderer* renderer)
{

}

ImageResource* RenderGraph::QueryImage(const std::string& name)
{
	auto it = imageBind.find(name);
	if (it != imageBind.end())
	{
		size_t index = it->second;
		if (index < imgResource.size())
		{
			return &imgResource[index];
		}
	}
	return nullptr;
}

BufferResource* RenderGraph::QueryBuffer(const std::string& name)
{
	auto it = bufferBind.find(name);
	if (it != bufferBind.end())
	{
		size_t index = it->second;
		if (index < buffResource.size())
		{
			return &buffResource[index];
		}
	}

	return nullptr;
}

ShaderDesc* RenderGraph::QueryShader(const std::string& name)
{
	auto it = shaderBind.find(name);
	if (it != shaderBind.end())
	{
		size_t index = it->second;
		if (index < shaders.size())
		{
			return &shaders[index];
		}
	}
	return nullptr;
}
