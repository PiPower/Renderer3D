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

static VkImageType getVkImageType(VkImageViewType viewType) {
	switch (viewType) {
	case VK_IMAGE_VIEW_TYPE_1D:
	case VK_IMAGE_VIEW_TYPE_1D_ARRAY:
		return VK_IMAGE_TYPE_1D;

	case VK_IMAGE_VIEW_TYPE_2D:
	case VK_IMAGE_VIEW_TYPE_2D_ARRAY:
	case VK_IMAGE_VIEW_TYPE_CUBE:
	case VK_IMAGE_VIEW_TYPE_CUBE_ARRAY:
		return VK_IMAGE_TYPE_2D;

	case VK_IMAGE_VIEW_TYPE_3D:
		return VK_IMAGE_TYPE_3D;

	default:
		return VK_IMAGE_TYPE_MAX_ENUM;
	}
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

void RenderGraph::Compile(Renderer* renderer)
{
	AllocateResources(renderer);
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
	PipelineInputDesc inputDesc = CreatePipelineInput(renderPass);
	PipelineRenderingDesc renderDesc = CreatePipelineRendering(renderPass);

	VkDynamicState dynamicStates[2] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	VkPipelineDynamicStateCreateInfo dynamicState{};
	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.dynamicStateCount = 2;
	dynamicState.pDynamicStates = dynamicStates;

	VkGraphicsPipelineCreateInfo pipelineInfo = {};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.pNext = &renderDesc.info;
	pipelineInfo.stageCount = (uint32_t)shaderStages.size();
	pipelineInfo.pStages = shaderStages.data();
	pipelineInfo.pVertexInputState = &inputDesc.info;
	pipelineInfo.pInputAssemblyState = &renderPass->asmInfo;
	pipelineInfo.pTessellationState = nullptr;
	pipelineInfo.pViewportState = &renderPass->vpInfo;
	pipelineInfo.pRasterizationState = &renderPass->rasterInfo;
	pipelineInfo.pMultisampleState = &renderPass->multisampling;
	pipelineInfo.pDepthStencilState = &renderPass->depthInfo;
	pipelineInfo.pColorBlendState = &renderPass->colorBlending;
	pipelineInfo.pDynamicState = &dynamicState;
	pipelineInfo.layout = nullptr;
	pipelineInfo.renderPass = VK_NULL_HANDLE;
	pipelineInfo.subpass = 0;
	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
	pipelineInfo.basePipelineIndex = 0;

	VkPipeline pipeline;
	VkResult res = vkCreateGraphicsPipelines(renderer->GetDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline);
	if (res != VK_SUCCESS)
	{
		throw std::runtime_error("Pipeline creation failed\n");
	}


	return pipeline;
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

PipelineInputDesc RenderGraph::CreatePipelineInput(RenderPass* renderPass)
{
	PipelineInputDesc inputDesc = {};
	uint32_t location = 0;
	for (size_t i = 0; i < renderPass->vertexBuffers.size(); i++)
	{
		const BufferResource& res = *renderPass->vertexBuffers[i];
		inputDesc.bindings.push_back({});

		VkVertexInputBindingDescription* bindingInfo = &inputDesc.bindings.back();
		bindingInfo->binding = (uint32_t)i;
		bindingInfo->inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		bindingInfo->stride = (uint32_t)res.size;

		for (size_t j = 0; j < res.formatOffsets.size(); j++)
		{
			inputDesc.attributes.push_back({});

			VkVertexInputAttributeDescription* attInfo = &inputDesc.attributes.back();
			attInfo->binding = (uint32_t)i;
			attInfo->location = location;
			attInfo->format = res.vertexInputFormats[j];
			attInfo->offset = res.formatOffsets[j];
			location++;
		}
	}

	inputDesc.info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	inputDesc.info.vertexBindingDescriptionCount = (uint32_t)inputDesc.bindings.size();
	inputDesc.info.pVertexBindingDescriptions = inputDesc.bindings.data();
	inputDesc.info.vertexAttributeDescriptionCount = (uint32_t)inputDesc.attributes.size();
	inputDesc.info.pVertexAttributeDescriptions = inputDesc.attributes.data();

	return inputDesc;
}

PipelineRenderingDesc RenderGraph::CreatePipelineRendering(RenderPass* renderPass)
{
	PipelineRenderingDesc render = {};
	render.info.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
	return render;
}

void RenderGraph::AllocateResources(Renderer* renderer)
{
	VkSurfaceCapabilitiesKHR capabilities = renderer->GetSwapchainCapabilities();
	for (size_t i = 0; i < imgResource.size(); i++)
	{
		ImageResource* img = &imgResource[i];
		VkImageCreateInfo imgInfo = {};
		VkImageViewCreateInfo viewInfo = {};
		VkExtent3D imgExtent = {};
		imgExtent.width = img->width == SWAPCHAIN_RELATIVE ? capabilities.currentExtent.width : img->width;
		imgExtent.height = img->height == SWAPCHAIN_RELATIVE ? capabilities.currentExtent.width : img->height;
		imgExtent.depth = 1;

		imgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imgInfo.pNext = nullptr;
		imgInfo.flags = 0;
		imgInfo.imageType = getVkImageType(img->viewType);
		imgInfo.format = img->format;
		imgInfo.extent = imgExtent;
		imgInfo.mipLevels = 1;
		imgInfo.arrayLayers = img->layers;
		imgInfo.samples = img->samples;
		imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imgInfo.usage = img->aux_usage;
		imgInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imgInfo.queueFamilyIndexCount = 0;
		imgInfo.pQueueFamilyIndices = nullptr;
		imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.flags;
		viewInfo.image = VK_NULL_HANDLE; // set inside renderer->AllocateImage
		viewInfo.viewType = img->viewType;
		viewInfo.format = img->format;
		viewInfo.components = VkComponentMapping{
			.r = VK_COMPONENT_SWIZZLE_IDENTITY, 
			.g = VK_COMPONENT_SWIZZLE_IDENTITY, 
			.b = VK_COMPONENT_SWIZZLE_IDENTITY, 
			.a =VK_COMPONENT_SWIZZLE_IDENTITY };
		viewInfo.subresourceRange;

		Image imgRes = renderer->AllocateImage(imgInfo, viewInfo);
	}

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

void RenderGraph::DescribeVertexBuffer(
	const std::string& name, 
	uint32_t stride, 
	const std::vector<VkFormat>& vertexInputFormats, 
	const std::vector<uint32_t>& formatOffsets)
{
	if (vertexInputFormats.size() != formatOffsets.size())
	{
		throw std::runtime_error("vertexInputFormats != formatOffsets\n");
	}

	BufferResource* buff = QueryBuffer(name);
	if (buff->isDefined > 0)
	{
		throw std::runtime_error("vertex buffer redefinition\n");
	}
	buff->isDefined = 1;
	buff->size = stride;
	buff->vertexInputFormats = vertexInputFormats;
	buff->formatOffsets = formatOffsets;
}

void RenderGraph::DescribeImage(
	const std::string& name, 
	uint32_t width, 
	uint32_t height, 
	uint32_t layers, 
	VkFormat format, 
	VkSampleCountFlagBits samples, 
	VkImageViewType viewType)
{
	ImageResource* img = QueryImage(name);
	if (img->isDefined > 0)
	{
		throw std::runtime_error("image redefinition\n");
	}
	if (width == SWAPCHAIN_RELATIVE || height == SWAPCHAIN_RELATIVE)
	{
		swcRelativeImages.push_back(img);
	}

	img->isDefined = 1;
	img->width = width;
	img->height = height;
	img->layers = layers;
	img->format = format;
	img->samples = samples;
	img->viewType = viewType;
}
