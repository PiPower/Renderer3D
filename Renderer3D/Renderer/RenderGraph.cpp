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
{}

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
	std::vector<VkImageLayout> initLayout(execGraph.imageResources.size(), VK_IMAGE_LAYOUT_UNDEFINED);
	for (size_t i = 0; i < renderPasses.size(); ++i)
	{
		RenderingPipeline pipeline = CompilePipeline(&renderPasses[i]);
		RenderResources passResources = CreateRenderResources(&renderPasses[i]);
		RenderInfoStruct renderInfo = CreateRenderInfoForPass(passResources);
		FindInitialLayoutForImages(&renderPasses[i], &initLayout);
		FillDescriptorSets(&renderPasses[i], &pipeline.sets);

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

	InitializeLayouts(initLayout);

	if (!displayImageRes)
	{
		MessageBox(NULL, L"\nThere is no image marked as display\n", NULL, MB_OK);
		exit(-1);
	}
	displayImage = &execGraph.imageResources[imageLookup.find(displayImageRes)->second];
}

void RenderGraph::MarkAsDisplayImage(const std::string& name)
{
	displayImageRes = QueryImage(name);
	if (displayImageRes == nullptr)
	{
		throw std::runtime_error("image does not exist\n");
	}
}

void RenderGraph::FindInitialLayoutForImages(
	RenderPass* renderPass,
	std::vector<VkImageLayout>* layouts)
{
	std::vector<VkImageLayout>& layoutsRef = *layouts;

	for (size_t i = 0; i < renderPass->outputImages.size(); i++)
	{
		const ImageResource* imgRes = renderPass->usedImages[renderPass->outputImages[i].i];
		size_t imgIdx = imageLookup.find(imgRes)->second;

		if (layoutsRef[imgIdx] == VK_IMAGE_LAYOUT_UNDEFINED)
		{
			layoutsRef[imgIdx] = renderPass->outputImages[i].layout;
		}

	}

	if (renderPass->depthImage.layout != VK_IMAGE_LAYOUT_UNDEFINED)
	{
		const ImageResource* imgRes = renderPass->usedImages[renderPass->depthImage.i];
		size_t imgIdx = imageLookup.find(imgRes)->second;

		if (layoutsRef[imgIdx] == VK_IMAGE_LAYOUT_UNDEFINED)
		{
			layoutsRef[imgIdx] = renderPass->depthImage.layout;
		}
	}

	for (size_t i = 0; i < renderPass->textureImages.size(); i++)
	{
		const ImageResource* imgRes = renderPass->usedImages[renderPass->textureImages[i].i];
		size_t imgIdx = imageLookup.find(imgRes)->second;

		if (layoutsRef[imgIdx] == VK_IMAGE_LAYOUT_UNDEFINED)
		{
			layoutsRef[imgIdx] = renderPass->textureImages[i].layout;
		}

	}
}

void RenderGraph::InitializeLayouts(const std::vector<VkImageLayout>& initialLayouts)
{
	std::vector<VkImageMemoryBarrier> imgBarriers(initialLayouts.size());
	for (size_t i = 0; i < imgBarriers.size(); i++)
	{
		VkImageMemoryBarrier* barrier = &imgBarriers[i];
		*barrier = {};
		barrier->sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier->srcAccessMask = 0;
		barrier->dstAccessMask = 0;
		barrier->oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		barrier->newLayout = initialLayouts[i];
		barrier->srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier->dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier->image = execGraph.imageResources[i].img;
		barrier->subresourceRange.aspectMask = GetAspectMask(imgResource[i]->format);
		barrier->subresourceRange.baseMipLevel = 0;
		barrier->subresourceRange.levelCount = 1;
		barrier->subresourceRange.baseArrayLayer = 0;
		barrier->subresourceRange.layerCount = imgResource[i]->layers;

		execGraph.imageResources[i].currLayout = initialLayouts[i];
	}

	VkCommandBuffer cmdBuff = execGraph.gfxCmdBuffers[0];

	VkCommandBufferBeginInfo cmdInfo = { };
	cmdInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	cmdInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &cmdBuff;


	EXIT_ON_VK_ERROR(vkResetCommandBuffer(cmdBuff, 0));
	EXIT_ON_VK_ERROR(vkBeginCommandBuffer(cmdBuff, &cmdInfo));

	vkCmdPipelineBarrier(cmdBuff, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 
															0, 0, nullptr, 0, nullptr, (uint32_t)imgBarriers.size(), imgBarriers.data());
	EXIT_ON_VK_ERROR(vkEndCommandBuffer(cmdBuff));
	renderer->RunCommandsAndSync(submitInfo);

}

RenderingPipeline RenderGraph::CompilePipeline(RenderPass* renderPass)
{
	RenderingPipeline pipelineOut = {};
	pipelineOut.renderFn = renderPass->renderFn;
	pipelineOut.setLayouts = CreateSetLayouts(renderPass);
	pipelineOut.layout = renderer->CreatePipelineLayout(pipelineOut.setLayouts);
	pipelineOut.descPool = CreateDescriptorPool(renderPass, pipelineOut.setLayouts);

	VkDescriptorSetAllocateInfo descAlloc = {};
	descAlloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	descAlloc.descriptorPool = pipelineOut.descPool;
	descAlloc.descriptorSetCount = 3;
	descAlloc.pSetLayouts = pipelineOut.setLayouts.data();
	pipelineOut.sets.resize(3);
	EXIT_ON_VK_ERROR(vkAllocateDescriptorSets(renderer->GetDevice(), &descAlloc, pipelineOut.sets.data()));

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
		const ImageResource* imgRes = renderPass->usedImages[renderPass->outputImages[i].i];
		size_t imgIdx = imageLookup.find(imgRes)->second;
		frameResources.colorImages.push_back(&execGraph.imageResources[imgIdx]);
	}
	if (renderPass->depthImage.layout != VK_IMAGE_LAYOUT_UNDEFINED)
	{
		const ImageResource* imgRes = renderPass->usedImages[renderPass->depthImage.i];
		size_t imgIdx = imageLookup.find(imgRes)->second;
		frameResources.depthImage = &execGraph.imageResources[imgIdx];
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
		size_t imageId = renderPass->outputImages[i].i;
		render.outputFormats.push_back(renderPass->usedImages[imageId]->format);
	}

	render.info.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
	render.info.pNext = nullptr;
	render.info.viewMask = 0;
	render.info.colorAttachmentCount = (uint32_t)render.outputFormats.size();
	render.info.pColorAttachmentFormats = render.outputFormats.data();
	if (renderPass->depthImage.layout != VK_IMAGE_LAYOUT_UNDEFINED)
	{
		const ImageResource* imgRes = renderPass->usedImages[renderPass->depthImage.i];
		render.info.depthAttachmentFormat = imgRes->format;
	}
	render.info.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;
	return render;
}

std::vector<VkDescriptorSetLayout> RenderGraph::CreateSetLayouts(RenderPass* renderPass)
{
	std::vector<VkDescriptorSetLayout> setLayouts(3);

	for (uint32_t i = 1; i <= static_cast<uint32_t>(BindLevel::PER_OBJECT); i++)
	{
		std::vector<VkDescriptorSetLayoutBinding> bindings = 
				CreateBufferBindings(renderPass->uniformBuffers, renderPass->textureImages, static_cast<BindLevel>(i));

		VkDescriptorSetLayoutCreateInfo setInfo = {};
		setInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		setInfo.bindingCount = (uint32_t)bindings.size();
		setInfo.pBindings = bindings.data();
		setLayouts[i-1] = renderer->CreateDescriptorSet(&setInfo);
	}
	return setLayouts;
}

VkDescriptorPool RenderGraph::CreateDescriptorPool(
	RenderPass* renderPass, 
	const std::vector<VkDescriptorSetLayout>& setLayouts)
{
	VkDescriptorPool pool;
	std::array<VkDescriptorPoolSize, 3> poolSizes;
	uint32_t unifrom = 0, uniformDynamic = 0;
	for (size_t i = 0; i < renderPass->uniformBuffers.size(); i++)
	{
		if (renderPass->uniformBuffers[i].isDynamic)
		{
			uniformDynamic++;
		}
		else
		{
			unifrom++;
		}
	}


	poolSizes[0] = {};
	poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSizes[0].descriptorCount = unifrom;


	poolSizes[1] = {};
	poolSizes[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
	poolSizes[1].descriptorCount = uniformDynamic;

	poolSizes[2] = {};
	poolSizes[2].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
	poolSizes[2].descriptorCount = (uint32_t)renderPass->textureImages.size();

	VkDescriptorPoolCreateInfo poolInfo = {};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.maxSets = 3;
	poolInfo.poolSizeCount = (uint32_t)poolSizes.size();
	poolInfo.pPoolSizes = poolSizes.data();

	EXIT_ON_VK_ERROR(vkCreateDescriptorPool(renderer->GetDevice(), &poolInfo, nullptr, &pool));
	return pool;
}

std::vector<VkDescriptorSetLayoutBinding> RenderGraph::CreateBufferBindings(
	const std::vector<UniformBuffer>& uniformBuffers,
	const std::vector<TextureImage>& textures,
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

	for (size_t i = 0; i < textures.size(); i++)
	{
		const TextureImage& texImg = textures[i];
		if (texImg.level != level)
		{
			continue;
		}
		VkDescriptorSetLayoutBinding setBind = {};
		setBind.binding = bindIdx++;
		setBind.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		setBind.descriptorCount = 1;
		setBind.stageFlags = texImg.stages;

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
		imgExtent.height = img->height == SWAPCHAIN_RELATIVE ? capabilities.currentExtent.height : img->height;
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
		imgInfo.usage = img->aux_usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
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
		buffInfo.usage = buff->usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		buffInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		buffInfo.queueFamilyIndexCount = 0;
		buffInfo.pQueueFamilyIndices = nullptr;

		VkMemoryPropertyFlagBits memoryVisibility = buff->isHostVisible ? VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT : VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
		VkMemoryPropertyFlagBits coherence = buff->isHostCoherent ? VK_MEMORY_PROPERTY_HOST_COHERENT_BIT : (VkMemoryPropertyFlagBits)0;
		VkMemoryPropertyFlagBits properties = (VkMemoryPropertyFlagBits)(memoryVisibility | coherence);

		Buffer buffRes = renderer->AllocateBuffer(buffInfo, properties);
		execGraph.bufferResources.push_back(buffRes);
	}

	VkSamplerCreateInfo samplerInfo = {};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.magFilter = VK_FILTER_NEAREST;
	samplerInfo.minFilter = VK_FILTER_NEAREST;
	samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.anisotropyEnable = VK_FALSE;
	samplerInfo.maxAnisotropy = 0;
	samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	samplerInfo.unnormalizedCoordinates = VK_FALSE;
	samplerInfo.compareEnable = VK_FALSE;
	samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

	EXIT_ON_VK_ERROR(vkCreateSampler(renderer->GetDevice(), &samplerInfo, nullptr, &sampler2D));

}

RenderInfoStruct RenderGraph::CreateRenderInfoForPass(const RenderResources& resources)
{
	RenderInfoStruct info = {};
	size_t descCount = resources.colorImages.size() + 1; // 1 for depth image if not present skip it
	info.attachments.resize(descCount);

	for (size_t i = 0; i < resources.colorImages.size(); i++)
	{
		VkRenderingAttachmentInfo* attachmentInfo = &info.attachments[i];
		attachmentInfo->sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
		attachmentInfo->imageView = resources.colorImages[i]->imgView;
		attachmentInfo->imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		attachmentInfo->resolveMode = VK_RESOLVE_MODE_NONE;
		attachmentInfo->resolveImageView = VK_NULL_HANDLE ;
		attachmentInfo->resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachmentInfo->loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachmentInfo->storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachmentInfo->clearValue.color = { 0.4, 0.9, 0.9, 1.0f };
	}
	if (resources.depthImage)
	{
		VkRenderingAttachmentInfo* depthAttInfo = &info.attachments[resources.colorImages.size()];
		depthAttInfo->sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
		depthAttInfo->imageView = resources.depthImage->imgView;
		depthAttInfo->imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
		depthAttInfo->resolveMode = VK_RESOLVE_MODE_NONE;
		depthAttInfo->resolveImageView = VK_NULL_HANDLE;
		depthAttInfo->resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		depthAttInfo->loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		depthAttInfo->storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depthAttInfo->clearValue.depthStencil = { 1.0f, 0 };
	}


	info.renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
	info.renderingInfo.renderArea = {
		{0 ,0},
		renderer->GetSwapchainCapabilities().currentExtent };
	info.renderingInfo.layerCount = 1;
	info.renderingInfo.viewMask = 0;
	info.renderingInfo.colorAttachmentCount = (uint32_t)resources.colorImages.size();
	info.renderingInfo.pColorAttachments = info.attachments.data();
	if (resources.depthImage)
	{
		info.renderingInfo.pDepthAttachment = info.attachments.data() + resources.colorImages.size();
	}
	info.renderingInfo.pStencilAttachment = nullptr;

	return info;
}

void RenderGraph::Render(void* args)
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
			execGraph.gfxCmdBuffers[i],
			args);

		EXIT_ON_VK_ERROR(vkEndCommandBuffer(execGraph.gfxCmdBuffers[i]));

		VkSubmitInfo submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &execGraph.gfxCmdBuffers[i];
		renderer->RunCommandsAndSync(submitInfo);
	}

	renderer->DisplayImageAndSync(displayImage->img, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
}

Image* RenderGraph::GetImage(const std::string& name)
{
	ImageResource* img = QueryImage(name);
	if (img == nullptr)
	{
		throw std::runtime_error("Image does not exist\n");
	}

	size_t imageIdx = imageLookup.find(img)->second;
	return &execGraph.imageResources[imageIdx];
}

void RenderGraph::RunPipeline(
	const RenderingPipeline& renderPipeline,
	const RenderResources& resources,
	RenderInfoStruct* renderInfo,
	VkCommandBuffer cmdBuffer,
	void* args)
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

	renderPipeline.renderFn(resources, cmdBuffer, &renderPipeline, args);

	vkCmdEndRendering(cmdBuffer);
}

void RenderGraph::FillDescriptorSets(
	RenderPass* renderPass, 
	std::vector<VkDescriptorSet>* sets)
{
	size_t writeSetCount = renderPass->uniformBuffers.size() + renderPass->textureImages.size() ;
	std::vector<VkWriteDescriptorSet> writeSets(writeSetCount);
	std::vector<VkDescriptorBufferInfo> descBuffInfos(renderPass->uniformBuffers.size());
	std::vector<VkDescriptorImageInfo> imgInfos(renderPass->textureImages.size());

	uint32_t perPassBind = 0, perMaterialBind = 0, perObjBind = 0;

	for (size_t i = 0; i < writeSets.size(); i++)
	{
		VkWriteDescriptorSet* writeSet = &writeSets[i];
		*writeSet = {};
		writeSet->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeSet->dstArrayElement = 0;
		BindLevel level;
		if (i < renderPass->uniformBuffers.size())
		{
			const BufferResource* buffResource = renderPass->usedBuffers[renderPass->uniformBuffers[i].i];
			size_t bufferIndex = bufferLookup[buffResource];

			VkDescriptorBufferInfo* buffInfo = &descBuffInfos[i];
			*buffInfo = {};
			buffInfo->buffer = execGraph.bufferResources[bufferIndex].buff;
			buffInfo->offset = 0;
			buffInfo->range = renderPass->uniformBuffers[i].size;

			writeSet->descriptorCount = 1;
			writeSet->descriptorType = renderPass->uniformBuffers[i].isDynamic ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC : VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			writeSet->pBufferInfo = buffInfo;
			level = renderPass->uniformBuffers[i].level;
		}
		else
		{
			size_t imgI = i - renderPass->uniformBuffers.size();
			const ImageResource* imgResource = renderPass->usedImages[renderPass->textureImages[imgI].i];
			size_t imgIndex = imageLookup[imgResource];

			VkDescriptorImageInfo* imgInfo = &imgInfos[imgI];
			*imgInfo = {};
			imgInfo->imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			imgInfo->imageView = execGraph.imageResources[imgIndex].imgView;
			imgInfo->sampler = sampler2D;

			writeSet->descriptorCount = 1;
			writeSet->descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			writeSet->pImageInfo = imgInfo;
			level = renderPass->textureImages[imgI].level;
		}

		switch (level)
		{
		case BindLevel::PER_PASS: 
			writeSet->dstSet = (*sets)[0];
			writeSet->dstBinding = perPassBind++;
			break;
		case BindLevel::PER_MATERIAL:
			writeSet->dstSet = (*sets)[1];
			writeSet->dstBinding = perMaterialBind++;
			break;
		case BindLevel::PER_OBJECT:
			writeSet->dstSet = (*sets)[2];
			writeSet->dstBinding = perObjBind++;
			break;
		default:
			MessageBox(NULL, L"Incorrect BindLevel", NULL, MB_OK);
			exit(-1);
			break;
		}
	}

	vkUpdateDescriptorSets(renderer->GetDevice(), (uint32_t)writeSets.size(), writeSets.data(), 0, nullptr);
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

char* RenderGraph::GetPtrToVisibleBuffer(const std::string& bufferName)
{
	const BufferResource* buffRes = QueryBuffer(bufferName);
	if (!buffRes)
	{
		throw std::runtime_error("Buffer not found\n");
	}
	size_t bufferId = bufferLookup.find(buffRes)->second;
	Buffer* buff = &execGraph.bufferResources[bufferId];
	if (!buff->mmap)
	{
		throw std::runtime_error("Buffer is not host vidible\n");
	}
	return buff->mmap;
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
	bool isHostVisible,
	bool isHostCoherent)
{
	if (QueryBuffer(name) != nullptr)
	{
		throw std::runtime_error("vertex buffer redefinition\n");
	}

	buffResource.emplace_back(new  BufferResource(1, isHostVisible, isHostCoherent,(VkBufferUsageFlags)0, (VkDeviceSize)size));
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
