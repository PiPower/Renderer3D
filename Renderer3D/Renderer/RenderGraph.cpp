#include "RenderGraph.hpp"
#include <Windows.h>
#include <stdexcept>
#include "ShaderCompiler.hpp"

#define STRINGIFY_(x) #x
#define STRINGIFY(x) STRINGIFY_(x)
#define EXIT_ON_VK_ERROR(expr){VkResult __result__ = (expr); if(__result__ != VK_SUCCESS){\
	MessageBox(NULL, L"vkResult is error\nLINE: " STRINGIFY(__LINE__) "\nFILE: " STRINGIFY(__FILE__), NULL, MB_OK); exit(-1); }}


template<typename BufferType>
static void FilterBuffers(
	const std::vector<BufferType>& searchedBuffers,
	const std::vector<const BufferResource*>& pointerHolder,
	const std::vector<Buffer>& bufferPool,
	const std::unordered_map<const BufferResource*, size_t>& bufferLookup,
	std::vector<const Buffer*>* destBuffer)
{
	for (size_t i = 0; i < searchedBuffers.size(); i++)
	{
		const BufferResource* buff = pointerHolder[searchedBuffers[i].i];
		size_t buffResIdx = bufferLookup.find(buff)->second;
		destBuffer->push_back(&bufferPool[buffResIdx]);
	}
}

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

static VkImageAspectFlags GetAspectMask(VkFormat format)
{
	VkImageAspectFlags aspectMask = 0;

	switch (format) {
		// Depth + stencil
	case VK_FORMAT_D16_UNORM_S8_UINT:
	case VK_FORMAT_D24_UNORM_S8_UINT:
	case VK_FORMAT_D32_SFLOAT_S8_UINT:
		aspectMask |= VK_IMAGE_ASPECT_DEPTH_BIT;
		aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
		break;

		// Depth only
	case VK_FORMAT_D16_UNORM:
	case VK_FORMAT_X8_D24_UNORM_PACK32:
	case VK_FORMAT_D32_SFLOAT:
		aspectMask |= VK_IMAGE_ASPECT_DEPTH_BIT;
		break;

		// Stencil only
	case VK_FORMAT_S8_UINT:
		aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
		break;

		// Everything else
	default:
		aspectMask |= VK_IMAGE_ASPECT_COLOR_BIT;
		break;
	}

	return aspectMask;
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

RenderGraph::RenderGraph()
	:
	renderer(nullptr)
{
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

void RenderGraph::Compile(Renderer* rendererInst)
{
	if (renderer)
	{
		throw std::runtime_error("Renderer is already bound\n");
	}
	renderer = rendererInst;

	AllocateResources();
	for (size_t i = 0; i < renderPasses.size(); ++i)
	{
		RenderingPipeline pipeline = CompilePipeline(&renderPasses[i]);
		RenderResources passResources = CreateRenderResources(&renderPasses[i]);
		RenderInfoStruct renderInfo = CreateRenderInfoForPass(passResources);

		execGraph.pipelines.push_back(std::move(pipeline));
		execGraph.renderResources.push_back(passResources);
		execGraph.renderInfo.push_back(std::move(renderInfo));
	}

	execGraph.gfxCmdPool = renderer->CreateGraphicsCommandPool();
	execGraph.gfxCmdBuffers.resize(renderPasses.size());

	VkCommandBufferAllocateInfo cmdBuffInfo = {};
	cmdBuffInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	cmdBuffInfo.commandPool = execGraph.gfxCmdPool;
	cmdBuffInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	cmdBuffInfo.commandBufferCount = (uint32_t)renderPasses.size();
	if (vkAllocateCommandBuffers(renderer->GetDevice(), &cmdBuffInfo, execGraph.gfxCmdBuffers.data()) != VK_SUCCESS)
	{
		MessageBox(NULL, L"\nCommand buffers creation error\n", NULL, MB_OK);
		exit(-1);
	}

}

RenderingPipeline RenderGraph::CompilePipeline(RenderPass* renderPass)
{
	RenderingPipeline pipelineOut = {};
	pipelineOut.renderFn = renderPass->renderFn;
	pipelineOut.setLayouts = CreateSetLayouts(renderPass);
	pipelineOut.layout = renderer->CreatePipelineLayout(pipelineOut.setLayouts);

	std::vector<VkPipelineShaderStageCreateInfo> shaderStages = CompileShaders(renderPass);
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
	pipelineInfo.layout = pipelineOut.layout;
	pipelineInfo.renderPass = VK_NULL_HANDLE;
	pipelineInfo.subpass = 0;
	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
	pipelineInfo.basePipelineIndex = 0;

	VkResult res = vkCreateGraphicsPipelines(renderer->GetDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipelineOut.pipeline);
	if (res != VK_SUCCESS)
	{
		throw std::runtime_error("Pipeline creation failed\n");
	}

	for (const IndexBuffer& indexBuffer : renderPass->indexBuffers)
	{
		pipelineOut.indexTypes.push_back(indexBuffer.dType);
	}
	return pipelineOut;
}

RenderResources RenderGraph::CreateRenderResources(RenderPass* renderPass)
{
	RenderResources frameResources = {};
	FilterBuffers(renderPass->uniformBuffers, renderPass->usedBuffers, execGraph.bufferResources, bufferLookup, &frameResources.uniformBuffers);
	FilterBuffers(renderPass->vertexBuffers, renderPass->usedBuffers, execGraph.bufferResources, bufferLookup, &frameResources.vertexBuffers);
	FilterBuffers(renderPass->indexBuffers, renderPass->usedBuffers, execGraph.bufferResources, bufferLookup, &frameResources.indexBuffers);

	for (size_t i = 0; i < renderPass->outputImages.size(); i++)
	{
		size_t imgIdx = imageLookup.find(renderPass->outputImages[i])->second;
		frameResources.colorImages.push_back(&execGraph.imageResources[imgIdx]);
	}

	return frameResources;
}

std::vector<VkPipelineShaderStageCreateInfo> RenderGraph::CompileShaders(RenderPass* renderPass)
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
		const VertexBuffer& vb = renderPass->vertexBuffers[i];
		inputDesc.bindings.push_back({});

		VkVertexInputBindingDescription* bindingInfo = &inputDesc.bindings.back();
		bindingInfo->binding = (uint32_t)i;
		bindingInfo->inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		bindingInfo->stride = vb.stride;

		for (size_t j = 0; j < vb.formatOffsets.size(); j++)
		{
			inputDesc.attributes.push_back({});

			VkVertexInputAttributeDescription* attInfo = &inputDesc.attributes.back();
			attInfo->binding = (uint32_t)i;
			attInfo->location = location;
			attInfo->format = vb.vertexInputFormats[j];
			attInfo->offset = vb.formatOffsets[j];
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
	for (size_t i = 0; i < renderPass->outputImages.size(); i++)
	{
		render.outputFormats.push_back(renderPass->outputImages[i]->format);
	}

	render.info.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
	render.info.pNext = nullptr;
	render.info.viewMask = 0;
	render.info.colorAttachmentCount = (uint32_t)render.outputFormats.size();
	render.info.pColorAttachmentFormats = render.outputFormats.data();
	render.info.depthAttachmentFormat = VK_FORMAT_UNDEFINED;
	render.info.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;
	return render;
}

std::vector<VkDescriptorSetLayout> RenderGraph::CreateSetLayouts(RenderPass* renderPass)
{
	std::vector<VkDescriptorSetLayout> setLayouts;

	for (uint32_t i = 1; i <= static_cast<uint32_t>(BindLevel::PER_OBJECT); i++)
	{
		std::vector<VkDescriptorSetLayoutBinding> bindings = 
				CreateBufferBindings(renderPass->uniformBuffers, static_cast<BindLevel>(i));

		VkDescriptorSetLayoutCreateInfo setInfo = {};
		setInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		setInfo.bindingCount = (uint32_t)bindings.size();
		setInfo.pBindings = bindings.data();
		setLayouts.push_back(renderer->CreateDescriptorSet(&setInfo));
	}
	return setLayouts;
}

std::vector<VkDescriptorSetLayoutBinding> RenderGraph::CreateBufferBindings(
	const std::vector<UniformBuffer>& uniformBuffers,
	BindLevel level)
{
	std::vector<VkDescriptorSetLayoutBinding> bindings;
	uint32_t bindIdx = 0;
	for (size_t i = 0; i < uniformBuffers.size(); i++)
	{
		const UniformBuffer& uniformBuffer = uniformBuffers[i];
		if (uniformBuffer.level != level)
		{
			continue;
		}
		VkDescriptorSetLayoutBinding setBind = {};
		setBind.binding = bindIdx++;
		setBind.descriptorType = uniformBuffer.isDynamic ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC : VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		setBind.descriptorCount = 1;
		setBind.stageFlags = uniformBuffer.stages;

		bindings.push_back(setBind);
	}

	return bindings;
}


void RenderGraph::AllocateResources()
{
	VkSurfaceCapabilitiesKHR capabilities = renderer->GetSwapchainCapabilities();
	for (size_t i = 0; i < imgResource.size(); i++)
	{
		ImageResource* img = imgResource[i];
		if (img->isDefined == 0)
		{
			execGraph.imageResources.push_back({});
			continue;
		}
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
		viewInfo.subresourceRange.aspectMask = GetAspectMask(img->format);
		viewInfo.subresourceRange.baseMipLevel = 0;
		viewInfo.subresourceRange.levelCount = 1;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.layerCount = img->layers;

		Image imgRes = renderer->AllocateImage(imgInfo, viewInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		execGraph.imageResources.push_back(imgRes);
	}

	for (size_t i = 0; i < buffResource.size(); i++)
	{
		const BufferResource* buff = buffResource[i];
		VkBufferCreateInfo buffInfo = {};
		buffInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		buffInfo.pNext = nullptr;
		buffInfo.flags = 0;
		buffInfo.size = buff->size;
		buffInfo.usage = buff->usage;
		buffInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		buffInfo.queueFamilyIndexCount = 0;
		buffInfo.pQueueFamilyIndices = nullptr;
		VkMemoryPropertyFlagBits memoryVisibility = buff->isHostVisible ? VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT : VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

		Buffer buffRes = renderer->AllocateBuffer(buffInfo, memoryVisibility);
		execGraph.bufferResources.push_back(buffRes);
	}
}

RenderInfoStruct RenderGraph::CreateRenderInfoForPass(const RenderResources& resources)
{
	renderer->GetSwapchainCapabilities().currentExtent;
	RenderInfoStruct info = {};
	info.outputAttachments.resize(resources.colorImages.size());

	info.renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
	info.renderingInfo.renderArea = {
		{0 ,0},
		renderer->GetSwapchainCapabilities().currentExtent };
	info.renderingInfo.layerCount = 1;
	info.renderingInfo.viewMask = 0;
	info.renderingInfo.colorAttachmentCount = (uint32_t)resources.colorImages.size();
	info.renderingInfo.pColorAttachments = info.outputAttachments.data();
	info.renderingInfo.pDepthAttachment = nullptr;
	info.renderingInfo.pStencilAttachment = nullptr;

	for (size_t i = 0; i < info.outputAttachments.size(); i++)
	{
		VkRenderingAttachmentInfo* attachmentInfo = &info.outputAttachments[i];
		attachmentInfo->sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
		attachmentInfo->imageView = resources.colorImages[i]->imgView;
		attachmentInfo->imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		attachmentInfo->resolveMode = VK_RESOLVE_MODE_NONE;
		attachmentInfo->resolveImageView = VK_NULL_HANDLE ;
		attachmentInfo->resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachmentInfo->loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachmentInfo->storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachmentInfo->clearValue.color = { 0, 0, 0, 1.0f };
		attachmentInfo->clearValue.depthStencil = { 1.0f, 0 };
	}

	return info;
}

void RenderGraph::Render()
{
	for (size_t i = 0; i < execGraph.pipelines.size(); i++)
	{
		VkCommandBufferBeginInfo cmdInfo = { };
		cmdInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		cmdInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		EXIT_ON_VK_ERROR(vkResetCommandBuffer(execGraph.gfxCmdBuffers[i], 0));
		EXIT_ON_VK_ERROR(vkBeginCommandBuffer(execGraph.gfxCmdBuffers[i], &cmdInfo));

		RunPipeline(
			execGraph.pipelines[i],
			execGraph.renderResources[i],
			&execGraph.renderInfo[i],
			execGraph.gfxCmdBuffers[i]);

		EXIT_ON_VK_ERROR(vkEndCommandBuffer(execGraph.gfxCmdBuffers[i]));

		VkSubmitInfo submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &execGraph.gfxCmdBuffers[i];
		renderer->RunCommandsAndSync(submitInfo);
	}
}

void RenderGraph::RunPipeline(
	const RenderingPipeline& renderPipeline,
	const RenderResources& resources,
	RenderInfoStruct* renderInfo,
	VkCommandBuffer cmdBuffer)
{
	std::vector<VkBuffer> vb(resources.vertexBuffers.size());
	std::vector<VkDeviceSize> vbOffsets(resources.vertexBuffers.size());
	for (size_t i = 0; i < vb.size(); i++)
	{
		vb[i] = resources.vertexBuffers[i]->buff;
		vbOffsets[i] = 0;
	}

	vkCmdBeginRendering(cmdBuffer, &renderInfo->renderingInfo);
	vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, renderPipeline.pipeline);
	vkCmdBindVertexBuffers(cmdBuffer, 0, (uint32_t)resources.vertexBuffers.size(), vb.data(), vbOffsets.data());
	vkCmdBindIndexBuffer(cmdBuffer, resources.indexBuffers[0]->buff, 0, renderPipeline.indexTypes[0]);

	renderPipeline.renderFn(resources, cmdBuffer);

	vkCmdEndRendering(cmdBuffer);
}

ImageResource* RenderGraph::QueryImage(const std::string& name)
{
	auto it = imageBind.find(name);
	if (it != imageBind.end())
	{
		size_t index = it->second;
		if (index < imgResource.size())
		{
			return imgResource[index];
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
			return buffResource[index];
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
			return shaders[index];
		}
	}
	return nullptr;
}

void RenderGraph::UploadDataToBuffer(
	const std::string& bufferName,
	uint64_t uploadSize,
	const char* src,
	uint64_t srcOffset,
	uint64_t dstOffset)
{
	if (!renderer)
	{
		throw std::runtime_error("Graph is not compiled, operation of data upload is not permitted\n");
	}

	auto buffIter = bufferBind.find(bufferName);
	if (buffIter == bufferBind.end())
	{
		throw std::runtime_error("Buffer is not found\n");

	}

	size_t bufferIdx = buffIter->second;
	renderer->UploadDataToBuffer(&execGraph.bufferResources[bufferIdx], src, uploadSize, srcOffset, dstOffset);
}

void RenderGraph::DescribeBuffer(
	const std::string& name, 
	uint64_t size,
	bool isHostVisible)
{
	if (QueryBuffer(name) != nullptr)
	{
		throw std::runtime_error("vertex buffer redefinition\n");
	}

	buffResource.emplace_back(new  BufferResource(1, (VkBufferUsageFlags)0, (VkDeviceSize)size, isHostVisible));
	bufferBind[name] = buffResource.size() - 1;
	bufferLookup[buffResource.back()] = buffResource.size() - 1;
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
	if (QueryImage(name) != nullptr)
	{
		throw std::runtime_error("image redefinition\n");
	}

	imgResource.push_back(new ImageResource);
	imageBind[name] = imgResource.size() - 1;
	imageLookup[imgResource.back()] = imgResource.size() - 1;

	ImageResource* img = imgResource.back();
	img->isDefined = 1;
	img->width = width;
	img->height = height;
	img->layers = layers;
	img->format = format;
	img->samples = samples;
	img->viewType = viewType;

	if (width == SWAPCHAIN_RELATIVE || height == SWAPCHAIN_RELATIVE)
	{
		swcRelativeImages.push_back(img);
	}
}

void RenderGraph::DescribeShader(
	const std::string& name, 
	const std::string& entryName, 
	const std::string& path)
{
	if (QueryShader(name) != nullptr)
	{
		throw std::runtime_error("image redefinition\n");
	}

	shaders.emplace_back(new ShaderDesc(name, entryName, path, (VkShaderStageFlagBits)0, VK_NULL_HANDLE));
	shaderBind[name] = shaders.size() - 1;
}
