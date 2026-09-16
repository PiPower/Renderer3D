#include "RenderPass.hpp"
#include "RenderGraph.hpp"
#include <stdexcept>

RenderPass::RenderPass(RenderGraph* rg, bool isGraphicsPass)
	: 
	rg(rg), isGraphicsPass(isGraphicsPass)
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
	rasterInfo.cullMode =  VK_CULL_MODE_BACK_BIT;
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
};
void RenderPass::AddTextureImage(const std::string& name)
{
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

	img->aux_usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	blendAttachmets.push_back({});
	VkPipelineColorBlendAttachmentState* blend = &blendAttachmets.back();
	blend->colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	blend->blendEnable = VK_FALSE;
	colorBlending.attachmentCount = (uint32_t)blendAttachmets.size();
}

void RenderPass::AddDepthImage(const std::string& name)
{
	depthInfo.depthTestEnable = VK_TRUE;
}

void RenderPass::AddUniformBuffer(
	const std::string& name,
	BindLevel level,
	bool isBufferDynamic)
{
	BufferResource* buf = rg->QueryBuffer(name);
	if (buf == nullptr)
	{
		throw std::runtime_error("Buffer with name '" + name + "' does not exist.");
	}

	buf->usage |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	usedBuffers.push_back(buf);
	uniformBuffers.emplace_back(usedBuffers.size()-1, level, isBufferDynamic);
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

	buf->usage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	usedBuffers.push_back(buf);
	vertexBuffers.emplace_back(usedBuffers.size() - 1, stride, vertexInputFormats, formatOffsets);
}

void RenderPass::AddIndexBuffer(const std::string& name)
{
	BufferResource* buf = rg->QueryBuffer(name);
	if (buf == nullptr)
	{
		throw std::runtime_error("Buffer with name '" + name + "' does not exist.");
	}
	
	buf->usage |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
	indexBuffers.push_back(buf);
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
