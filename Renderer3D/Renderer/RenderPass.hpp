#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <string>
#include <unordered_map>
#include "Resource.hpp"
#include <array>


struct VertexBuffer
{
	size_t i;
	uint32_t stride;
	std::vector<VkFormat> vertexInputFormats;
	std::vector<uint32_t> formatOffsets;
};

struct IndexBuffer
{
	size_t i;
	VkFormat format;
};

struct UniformBuffer
{
	size_t i;
	BindLevel level;
	VkShaderStageFlags stages;
	uint8_t isDynamic : 1;
};

struct BufferClass
{
	size_t offset;
	uint8_t isUniformBuffer : 1;
	uint8_t isVertexBuffer : 1;
	uint8_t isIndexBuffer : 1;
};

struct ImageClass
{
	size_t offset;
	uint8_t isInput : 1;
	uint8_t isOutput : 1;
	uint8_t isTexture : 1;
};

struct RenderResources
{
	const std::vector<const Buffer&>& vertexBuffers;
	const std::vector<const Buffer&>& indexBuffers;
	const std::vector<const Buffer&>& uniformBuffers;

	const std::vector<const Image&>& inputAttachments;
	const std::vector<const Image&>& outputAttachments;
	const std::vector<const Image&>& textures;
};

typedef void (*RenderFunction)(const RenderResources& args, VkCommandBuffer cmdBuff);

class RenderGraph;

class RenderPass
{
	friend class RenderGraph;

private:
	static constexpr size_t SH_VERTEX = 0;
	static constexpr size_t SH_FRAGMENT = 1;

public:
	RenderPass(RenderGraph* rg, bool isGraphicsPass);

	void AddTextureImage(const std::string& name);

	void AddInputImage(const std::string& name);

	void AddColorAttachment(const std::string& name);

	void AddDepthImage(const std::string& name);

	void AddUniformBuffer(
		const std::string& name, 
		BindLevel level,
		bool isBufferDynamic = false);

	void AddVertexBuffer(
		const std::string& name,
		uint32_t stride,
		const std::vector<VkFormat>& vertexInputFormats,
		const std::vector<uint32_t>& formatOffsets);

	void AddIndexBuffer(const std::string& name);

	void AddVertexShader(const std::string& name);

	void AddFragmentShader(const std::string& name);

	void BindResourceToShader(
		const std::string& shaderName,
		const std::vector<std::string>& buffers);

	inline void SetRenderFunction(RenderFunction rn) { renderFn = rn; }
//  rendering pipeline settings
	inline RenderPass& SetTopology(VkPrimitiveTopology topology) { asmInfo.topology = topology; return *this; }

	inline RenderPass& SetDepthClampEnable(VkBool32 enable) { rasterInfo.depthClampEnable = enable; return *this; }

	inline RenderPass& SetRasterizerDiscardEnable(VkBool32 enable) { rasterInfo.rasterizerDiscardEnable = enable; return *this; }

	inline RenderPass& SetPolygonMode(VkPolygonMode mode) { rasterInfo.polygonMode = mode; return *this; }

	inline RenderPass& SetCullMode(VkCullModeFlags mode) { rasterInfo.cullMode = mode; return *this; }

	inline RenderPass& SetFrontFace(VkFrontFace frontFace) { rasterInfo.frontFace = frontFace; return *this; }

	inline RenderPass& SetDepthBiasEnable(VkBool32 enable) { rasterInfo.depthBiasEnable = enable; return *this; }

	inline RenderPass& SetDepthBiasConstantFactor(float factor) { rasterInfo.depthBiasConstantFactor = factor; return *this; }

	inline RenderPass& SetDepthBiasClamp(float clamp) { rasterInfo.depthBiasClamp = clamp; return *this; }

	inline RenderPass& SetDepthBiasSlopeFactor(float factor) { rasterInfo.depthBiasSlopeFactor = factor; return *this; }

	inline RenderPass& SetLineWidth(float width) { rasterInfo.lineWidth = width; return *this; }

	inline RenderPass& SetSampleShadingEnable(VkBool32 enable) { multisampling.sampleShadingEnable = enable; return *this; }

	inline RenderPass& SetRasterizationSamples(VkSampleCountFlagBits samples) { multisampling.rasterizationSamples = samples; return *this; }

	inline RenderPass& SetMinSampleShading(float value) { multisampling.minSampleShading = value; return *this; }

	inline RenderPass& SetSampleMask(const VkSampleMask* mask) { multisampling.pSampleMask = mask; return *this; }

	inline RenderPass& SetAlphaToCoverageEnable(VkBool32 enable) { multisampling.alphaToCoverageEnable = enable; return *this; }

	inline RenderPass& SetAlphaToOneEnable(VkBool32 enable) { multisampling.alphaToOneEnable = enable; return *this; }

	inline RenderPass& SetLogicOpEnable(VkBool32 enable) { colorBlending.logicOpEnable = enable; return *this; }

	inline RenderPass& SetLogicOp(VkLogicOp op) { colorBlending.logicOp = op; return *this; }

	inline RenderPass& SetBlendConstant0(float value) { colorBlending.blendConstants[0] = value; return *this; }

	inline RenderPass& SetBlendConstant1(float value) { colorBlending.blendConstants[1] = value; return *this; }

	inline RenderPass& SetBlendConstant2(float value) { colorBlending.blendConstants[2] = value; return *this; }

	inline RenderPass& SetBlendConstant3(float value) { colorBlending.blendConstants[3] = value; return *this; }

	void SetBlendEnable(size_t attachmentIdx, VkBool32 enable);

	void SetColorWriteMask(size_t attachmentIdx, VkColorComponentFlags mask);

	void SetSrcColorBlendFactor(size_t attachmentIdx, VkBlendFactor factor);

	void SetDstColorBlendFactor(size_t attachmentIdx, VkBlendFactor factor);

	void SetColorBlendOp(size_t attachmentIdx, VkBlendOp op);

	void SetSrcAlphaBlendFactor(size_t attachmentIdx, VkBlendFactor factor);

	void SetDstAlphaBlendFactor(size_t attachmentIdx, VkBlendFactor factor);

	void SetAlphaBlendOp(size_t attachmentIdx, VkBlendOp op);

	inline RenderPass& SetDepthWriteEnable(VkBool32 enable) { depthInfo.depthWriteEnable = enable; return *this; }

	inline RenderPass& SetDepthCompareOp(VkCompareOp op) { depthInfo.depthCompareOp = op; return *this; }

	inline RenderPass& SetDepthBoundsTestEnable(VkBool32 enable) { depthInfo.depthBoundsTestEnable = enable; return *this; }

	inline RenderPass& SetStencilFront(VkStencilOpState state) { depthInfo.front = state; return *this; }

	inline RenderPass& SetStencilBack(VkStencilOpState state) { depthInfo.back = state; return *this; }

	inline RenderPass& SetMinDepthBounds(float value) { depthInfo.minDepthBounds = value; return *this; }

	inline RenderPass& SetMaxDepthBounds(float value) { depthInfo.maxDepthBounds = value; return *this; }

	inline RenderPass& SetStencilFrontFailOp(VkStencilOp op) { depthInfo.front.failOp = op; return *this; }

	inline RenderPass& SetStencilFrontPassOp(VkStencilOp op) { depthInfo.front.passOp = op; return *this; }

	inline RenderPass& SetStencilFrontDepthFailOp(VkStencilOp op) { depthInfo.front.depthFailOp = op; return *this; }

	inline RenderPass& SetStencilFrontCompareOp(VkCompareOp op) { depthInfo.front.compareOp = op; return *this; }

	inline RenderPass& SetStencilFrontCompareMask(uint32_t mask) { depthInfo.front.compareMask = mask; return *this; }

	inline RenderPass& SetStencilFrontWriteMask(uint32_t mask) { depthInfo.front.writeMask = mask; return *this; }

	inline RenderPass& SetStencilFrontReference(uint32_t reference) { depthInfo.front.reference = reference; return *this; }

	inline RenderPass& SetStencilBackFailOp(VkStencilOp op) { depthInfo.back.failOp = op; return *this; }

	inline RenderPass& SetStencilBackPassOp(VkStencilOp op) { depthInfo.back.passOp = op; return *this; }

	inline RenderPass& SetStencilBackDepthFailOp(VkStencilOp op) { depthInfo.back.depthFailOp = op; return *this; }

	inline RenderPass& SetStencilBackCompareOp(VkCompareOp op) { depthInfo.back.compareOp = op; return *this; }

	inline RenderPass& SetStencilBackCompareMask(uint32_t mask) { depthInfo.back.compareMask = mask; return *this; }

	inline RenderPass& SetStencilBackWriteMask(uint32_t mask) { depthInfo.back.writeMask = mask; return *this; }

	inline RenderPass& SetStencilBackReference(uint32_t reference) { depthInfo.back.reference = reference; return *this; }


private:
	// resource related
	RenderFunction renderFn;
	RenderGraph* rg;
	bool isGraphicsPass;
	std::vector<const BufferResource*> usedBuffers;
	std::vector<VertexBuffer> vertexBuffers;
	std::vector<IndexBuffer> indexBuffers;
	std::vector<UniformBuffer> uniformBuffers;
	std::unordered_map<std::string, BufferClass> bufferBindings;

	std::vector<const ImageResource*> inputImages;
	std::vector<const ImageResource*> outputImages;
	std::vector<const ImageResource*> textureImages;
	std::unordered_map<std::string, ImageClass> imageBindings;

	std::array<std::string, 2> shaderStages;

	// pipeline description
	VkPipelineInputAssemblyStateCreateInfo  asmInfo;
	VkPipelineViewportStateCreateInfo vpInfo;
	VkPipelineRasterizationStateCreateInfo rasterInfo;
	VkPipelineMultisampleStateCreateInfo multisampling;
	VkPipelineColorBlendStateCreateInfo colorBlending;
	VkPipelineDepthStencilStateCreateInfo depthInfo;
	// pipeline description related data
	std::vector<VkPipelineColorBlendAttachmentState> blendAttachmets;
};
