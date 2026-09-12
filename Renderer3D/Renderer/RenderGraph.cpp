#include "RenderGraph.hpp"
#include <Windows.h>
#include <stdexcept>
#include "ShaderCompiler.hpp"

static inline constexpr shaderc_shader_kind extendKind(
	VkShaderStageFlags collection,
	VkShaderStageFlags chosenStage,
	shaderc_shader_kind rcCollection,
	shaderc_shader_kind rcStage)
{
	if (collection & chosenStage)
	{
		return (shaderc_shader_kind)(rcCollection | rcStage);
	}
	return rcCollection;
}

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
		shaders.back().bytecode = VK_NULL_HANDLE;
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
	for (size_t i = 0; i < renderPasses.size(); ++i)
	{
		CompilePipeline(renderer, &renderPasses[i]);
	}
}

VkPipeline RenderGraph::CompilePipeline(
	Renderer* renderer, 
	RenderPass* renderPass)
{
	
	std::vector<VkPipelineShaderStageCreateInfo> shaderStages = CompileShaders(renderer, renderPass);

	VkGraphicsPipelineCreateInfo pipelineInfo = {};
	pipelineInfo.stageCount = (uint32_t)shaderStages.size();
	pipelineInfo.pStages = shaderStages.data();

	return VkPipeline();
}

std::vector<VkPipelineShaderStageCreateInfo> RenderGraph::CompileShaders(
	Renderer* renderer,
	RenderPass* renderPass)
{
	ShaderCompiler compiler;
	std::vector<VkPipelineShaderStageCreateInfo> shaderInfo;
	for (size_t i = 0; i < renderPass->shaderStages.size(); i++)
	{
		if (renderPass->shaderStages[i] != "")
		{
			ShaderDesc* desc = QueryShader(renderPass->shaderStages[i]);
			if (desc->bytecode == VK_NULL_HANDLE)
			{
				shaderc_shader_kind stages = {};
				stages = extendKind(desc->stages, VK_SHADER_STAGE_VERTEX_BIT, stages, shaderc_vertex_shader);
				stages = extendKind(desc->stages, VK_SHADER_STAGE_FRAGMENT_BIT, stages, shaderc_fragment_shader);
				desc->bytecode = compiler.CompileShaderFromPath(renderer->GetDevice(), nullptr, desc->path.c_str(), desc->entryName.c_str(), stages, {});
			}
			shaderInfo.push_back({});
			VkPipelineShaderStageCreateInfo* info = &shaderInfo.back();
			info->sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			info->stage = desc->stages;
			info->module = desc->bytecode;
			info->pName = desc->entryName.c_str();
		}
	}

	return shaderInfo;
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
