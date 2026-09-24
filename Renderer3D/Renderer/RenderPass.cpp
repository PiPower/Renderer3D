#include "RenderPass.hpp"
#include "RenderGraph.hpp"
#include <stdexcept>

RenderPass::RenderPass(RenderGraph* rg, bool isGraphicsPass)
	: 
	rg(rg), isGraphicsPass(isGraphicsPass), renderFn(nullptr), depthImage(0, 0, VK_IMAGE_LAYOUT_UNDEFINED)
{
	asmInfo = {};
	asmInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	asmInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

	vpInfo = {};
	vpInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	vpInfo.viewportCount = 1;
	vpInfo.scissorCount = 1;

	rasterInfo = {};
	rasterInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterInfo.depthClampEnable = VK_FALSE;
	rasterInfo.rasterizerDiscardEnable = VK_FALSE;
	rasterInfo.polygonMode = VK_POLYGON_MODE_FILL;
	rasterInfo.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rasterInfo.depthBiasEnable = VK_FALSE;
	rasterInfo.depthBiasConstantFactor = 0.0f;
	rasterInfo.depthBiasClamp = 0.0f;
	rasterInfo.depthBiasSlopeFactor = 0.0f;
	rasterInfo.lineWidth = 1.0f;

	multisampling = {};
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.sampleShadingEnable = VK_FALSE;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisampling.minSampleShading = 1.0f;
	multisampling.pSampleMask = nullptr;
	multisampling.alphaToCoverageEnable = VK_FALSE;
	multisampling.alphaToOneEnable = VK_FALSE;

	colorBlending = {};
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.logicOp = VK_LOGIC_OP_COPY;
	colorBlending.attachmentCount = 0;
	colorBlending.pAttachments = blendAttachmets.data();
	colorBlending.blendConstants[0] = 0.0f;
	colorBlending.blendConstants[1] = 0.0f;
	colorBlending.blendConstants[2] = 0.0f;
	colorBlending.blendConstants[3] = 0.0f;

	depthInfo = {};
	depthInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthInfo.depthTestEnable = VK_FALSE;
	depthInfo.depthWriteEnable = VK_TRUE;
	depthInfo.depthCompareOp = VK_COMPARE_OP_LESS;
	depthInfo.depthBoundsTestEnable = VK_FALSE;
	depthInfo.stencilTestEnable = VK_FALSE;
}

void RenderPass::AddTextureImage(
	const std::string& name,
	uint32_t imgCount,
	BindLevel level,
	VkShaderStageFlags stageFlags)
{
	ImageResource* img = rg->QueryImage(name);
	if (img == nullptr)
	{
		throw std::runtime_error("Image does not exist\n");
	}
	if (imageBindings.find(name) != imageBindings.end())
	{
		throw std::runtime_error("Image is already bound\n");
	}

	img->aux_usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
	usedImages.push_back(img);
	imageBindings[name] = ImageClass(textureImages.size(), 0, 0, 1, 0, 0);
	textureImages.emplace_back(usedImages.size() - 1, imgCount, level, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, stageFlags);
}


void RenderPass::AddInputImage(const std::string& name)
{
}

void RenderPass::AddColorAttachment(const std::string& name)
{
	ImageResource* img = rg->QueryImage(name);
	if (img == nullptr)
	{
		throw std::runtime_error("Image does not exist\n");
	}
	if (imageBindings.find(name) != imageBindings.end())
	{
		throw std::runtime_error("Image is already bound\n");
	}
	img->aux_usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;


	usedImages.push_back(img);
	imageBindings[name] = ImageClass(outputImages.size(), 0, 1, 0, 0, 0);
	outputImages.emplace_back(usedImages.size() - 1, blendAttachmets.size(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

	blendAttachmets.push_back({});
	VkPipelineColorBlendAttachmentState* blend = &blendAttachmets.back();
	blend->colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	blend->blendEnable = VK_FALSE;
	colorBlending.attachmentCount = (uint32_t)blendAttachmets.size();
	colorBlending.pAttachments = blendAttachmets.data(); // blendAttachmets may rellocate so we need to reassing pointer
}

void RenderPass::AddDepthImage(const std::string& name)
{
	ImageResource* img = rg->QueryImage(name);
	if (img == nullptr)
	{
		throw std::runtime_error("Image does not exist\n");
	}
	if (imageBindings.find(name) != imageBindings.end())
	{
		throw std::runtime_error("Image is already bound\n");
	}
	img->aux_usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;


	depthInfo.depthTestEnable = VK_TRUE;

	usedImages.push_back(img);
	imageBindings[name] = ImageClass(outputImages.size(), 0, 0, 0, 1, 0);
	VkImageLayout layout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
	if (img->format == VK_FORMAT_D16_UNORM_S8_UINT ||
		img->format == VK_FORMAT_D24_UNORM_S8_UINT ||
		img->format == VK_FORMAT_D32_SFLOAT_S8_UINT)
	{
		layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	}

	depthImage = ColorAttachmentImage{ usedImages.size() - 1, blendAttachmets.size(), layout };
}

void RenderPass::AddUniformBuffer(
	const std::string& name,
	uint32_t size,
	BindLevel level,
	bool isBufferDynamic)
{
	BufferResource* buf = rg->QueryBuffer(name);
	if (buf == nullptr)
	{
		throw std::runtime_error("Buffer with name '" + name + "' does not exist.");
	}

	auto bufferBind = bufferBindings.find(name);
	if (bufferBind != bufferBindings.end())
	{
		throw std::runtime_error("Buffer with name '" + bufferBind->first + "' is already bound to pipeline.");
	}
	if (buf->size < size)
	{
		throw std::runtime_error("Uniform buffer is too large");
	}

	buf->usage |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	usedBuffers.push_back(buf);
	uniformBuffers.emplace_back(usedBuffers.size()-1, size, level, VK_SHADER_STAGE_ALL, isBufferDynamic);
	bufferBindings[name] = { uniformBuffers.size() - 1, 1, 0, 0 };
}

void RenderPass::AddVertexBuffer(
	const std::string& name,
	uint32_t stride,
	const std::vector<VkFormat>& vertexInputFormats,
	const std::vector<uint32_t>& formatOffsets)
{
	if (vertexInputFormats.size() != formatOffsets.size())
	{
		throw std::runtime_error("vertexInputFormats != formatOffsets\n");
	}

	BufferResource* buf = rg->QueryBuffer(name);
	if (buf == nullptr)
	{
		throw std::runtime_error("Buffer with name '" + name + "' does not exist.");
	}

	auto bufferBind = bufferBindings.find(name);
	if (bufferBind != bufferBindings.end())
	{
		throw std::runtime_error("Buffer with name '" + bufferBind->first + "' is already bound to pipeline.");
	}

	buf->usage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	usedBuffers.push_back(buf);
	vertexBuffers.emplace_back(usedBuffers.size() - 1, stride, vertexInputFormats, formatOffsets);
	bufferBindings[name] = { uniformBuffers.size() - 1, 0, 1, 0 };
}

void RenderPass::AddIndexBuffer(
	const std::string& name,
	VkIndexType indexType)
{
	BufferResource* buf = rg->QueryBuffer(name);
	if (buf == nullptr)
	{
		throw std::runtime_error("Buffer with name '" + name + "' does not exist.");
	}

	auto bufferBind = bufferBindings.find(name);
	if (bufferBind != bufferBindings.end())
	{
		throw std::runtime_error("Buffer with name '" + bufferBind->first + "' is already bound to pipeline.");
	}
	
	buf->usage |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
	usedBuffers.push_back(buf);
	indexBuffers.emplace_back(usedBuffers.size() - 1, indexType);
	bufferBindings[name] = { uniformBuffers.size() - 1, 0, 0, 1 };
}

void RenderPass::AddVertexShader(const std::string& name)
{
	if (!shaderStages[SH_VERTEX].empty())
	{
		throw std::runtime_error("Vertex shader already set for this render pass.");
	}
	shaderStages[SH_VERTEX] = name;
	ShaderDesc* shader = rg->QueryShader(name); 
	if (shader == nullptr)
	{
		throw std::runtime_error("Shader with name '" + name + "' does not exist.");
	}
	shader->stages = (VkShaderStageFlagBits)(shader->stages | VK_SHADER_STAGE_VERTEX_BIT);
}

void RenderPass::AddFragmentShader(const std::string& name)
{
	if (!shaderStages[SH_FRAGMENT].empty())
	{
		throw std::runtime_error("Fragment shader already set for this render pass.");
	}

	shaderStages[SH_FRAGMENT] = name;
	rg->QueryShader(name); 

	ShaderDesc* shader = rg->QueryShader(name);
	if (shader == nullptr)
	{
		throw std::runtime_error("Shader with name '" + name + "' does not exist.");
	}
	shader->stages = (VkShaderStageFlagBits)(shader->stages | VK_SHADER_STAGE_FRAGMENT_BIT);
}

void RenderPass::BindResourceToShader(
	const std::string& shaderName, 
	const std::vector<std::string>& buffers)
{
}

void RenderPass::SetBlendEnable(size_t attachmentIdx, VkBool32 enable)
{
	if (blendAttachmets.size() <= attachmentIdx)
		throw std::runtime_error("Attachment with specified attachmentIdx does not exist\n");

	blendAttachmets[attachmentIdx].blendEnable = enable;
}

void RenderPass::SetColorWriteMask(size_t attachmentIdx, VkColorComponentFlags mask)
{
	if (blendAttachmets.size() <= attachmentIdx)
		throw std::runtime_error("Attachment with specified attachmentIdx does not exist\n");

	blendAttachmets[attachmentIdx].colorWriteMask = mask;
}

void RenderPass::SetSrcColorBlendFactor(size_t attachmentIdx, VkBlendFactor factor)
{
	if (blendAttachmets.size() <= attachmentIdx)
		throw std::runtime_error("Attachment with specified attachmentIdx does not exist\n");

	blendAttachmets[attachmentIdx].srcColorBlendFactor = factor;
}

void RenderPass::SetDstColorBlendFactor(size_t attachmentIdx, VkBlendFactor factor)
{
	if (blendAttachmets.size() <= attachmentIdx)
		throw std::runtime_error("Attachment with specified attachmentIdx does not exist\n");

	blendAttachmets[attachmentIdx].dstColorBlendFactor = factor;
}

void RenderPass::SetColorBlendOp(size_t attachmentIdx, VkBlendOp op)
{
	if (blendAttachmets.size() <= attachmentIdx)
		throw std::runtime_error("Attachment with specified attachmentIdx does not exist\n");

	blendAttachmets[attachmentIdx].colorBlendOp = op;
}

void RenderPass::SetSrcAlphaBlendFactor(size_t attachmentIdx, VkBlendFactor factor)
{
	if (blendAttachmets.size() <= attachmentIdx)
		throw std::runtime_error("Attachment with specified attachmentIdx does not exist\n");

	blendAttachmets[attachmentIdx].srcAlphaBlendFactor = factor;
}

void RenderPass::SetDstAlphaBlendFactor(size_t attachmentIdx, VkBlendFactor factor)
{
	if (blendAttachmets.size() <= attachmentIdx)
		throw std::runtime_error("Attachment with specified attachmentIdx does not exist\n");

	blendAttachmets[attachmentIdx].dstAlphaBlendFactor = factor;
}

void RenderPass::SetAlphaBlendOp(size_t attachmentIdx, VkBlendOp op)
{
	if (blendAttachmets.size() <= attachmentIdx)
		throw std::runtime_error("Attachment with specified attachmentIdx does not exist\n");

	blendAttachmets[attachmentIdx].alphaBlendOp = op;
}
