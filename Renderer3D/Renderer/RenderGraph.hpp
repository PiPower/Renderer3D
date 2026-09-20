#pragma once
#include <vulkan/vulkan.h>
#include <unordered_map>
#include "RenderPass.hpp"
#include "Renderer.hpp"

struct RenderingPipeline
{
	VkPipeline pipeline;
	VkPipelineLayout layout;
	VkDescriptorPool descPool;
	std::vector<VkDescriptorSetLayout> setLayouts;
	std::vector<VkDescriptorSet> sets;
	RenderFunction renderFn;
	std::vector<VkIndexType> indexTypes;
};

struct RenderInfoStruct
{
	VkRenderingInfo renderingInfo;
	std::vector<VkRenderingAttachmentInfo> outputAttachments;
};

struct ExecutionGraph
{
	std::vector<RenderingPipeline> pipelines;
	std::vector<Image> imageResources;
	std::vector<Buffer> bufferResources;
	std::vector<RenderInfoStruct> renderInfo;
	// resources are copies of all above handles, they are to be fed to their render steps
	std::vector<RenderResources> renderResources;
	VkCommandPool gfxCmdPool;
	std::vector<VkCommandBuffer> gfxCmdBuffers;
};

struct ShaderDesc
{
	std::string name;
	std::string entryName;
	std::string path;
	VkShaderStageFlagBits stages;
	VkShaderModule bytecode;
};

struct PipelineInputDesc
{
	VkPipelineVertexInputStateCreateInfo info = {};
	std::vector<VkVertexInputBindingDescription> bindings;
	std::vector<VkVertexInputAttributeDescription> attributes;
};

struct PipelineRenderingDesc
{
	VkPipelineRenderingCreateInfoKHR info;
	std::vector<VkFormat> outputFormats;
};

class RenderGraph
{
public:
	RenderGraph();

	RenderPass* CreateRenderPass(
		const std::string& name,
		bool isGraphicsPass);

	void Compile(Renderer* rendererInst);

	void MarkAsDisplayImage(const std::string& name);

	ImageResource* QueryImage(const std::string& name);

	BufferResource* QueryBuffer(const std::string& name);

	ShaderDesc* QueryShader(const std::string& name);

	char* GetPtrToVisibleBuffer(const std::string& bufferName);

	void UploadDataToBuffer(
		const std::string& bufferName,
		uint64_t uploadSize,
		const char* src,
		uint64_t srcOffset,
		uint64_t dstOffset);

	void DescribeBuffer(
		const std::string& name,
		uint64_t size,
		bool isHostVisible = false,
		bool isHostCoherent = false);

	void DescribeImage(
		const std::string& name,
		uint32_t width,
		uint32_t height,
		uint32_t layers,
		VkFormat format,
		VkSampleCountFlagBits samples,
		VkImageViewType viewType);

	void DescribeShader(
		const std::string& name,
		const std::string& entryName,
		const std::string& path);

	void Render(void* args);

	inline ExecutionGraph* GetExecutionGraph() { return &execGraph; }
private:
	RenderingPipeline CompilePipeline(RenderPass* renderPass);

	RenderResources CreateRenderResources(RenderPass* renderPass);

	std::vector<VkPipelineShaderStageCreateInfo> CompileShaders(RenderPass* renderPass);

	PipelineInputDesc CreatePipelineInput(RenderPass* renderPass);

	PipelineRenderingDesc CreatePipelineRendering(RenderPass* renderPass);

	std::vector<VkDescriptorSetLayout> CreateSetLayouts(RenderPass* renderPass);

	VkDescriptorPool CreateDescriptorPool(
		RenderPass* renderPass,
		const std::vector<VkDescriptorSetLayout>& setLayouts);

	std::vector<VkDescriptorSetLayoutBinding> CreateBufferBindings(
		const std::vector<UniformBuffer>& uniformBuffers,
		BindLevel level);

	void AllocateResources();

	RenderInfoStruct CreateRenderInfoForPass(const RenderResources& resources);

	void RunPipeline(
		const RenderingPipeline& renderPipeline,
		const RenderResources& resources,
		RenderInfoStruct* renderInfo,
		VkCommandBuffer cmdBuffer,
		void* args);

	void FillDescriptorSets(
		RenderPass* renderPass,
		std::vector<VkDescriptorSet>* sets);

	void FindInitialLayoutForImages(
		RenderPass* renderPass,
		std::vector<VkImageLayout>* layouts);

	void InitializeLayouts(const std::vector<VkImageLayout>& initialLayouts);
private:
	std::vector<RenderPass> renderPasses;
	std::vector<ImageResource*> imgResource;
	std::vector<BufferResource*> buffResource;
	std::vector<ShaderDesc*> shaders;

	std::unordered_map<const BufferResource*, size_t> bufferLookup;
	std::unordered_map<const ImageResource*, size_t> imageLookup;

	std::unordered_map<std::string, size_t> renderPassNames;
	std::unordered_map<std::string, size_t> bufferBind;
	std::unordered_map<std::string, size_t> imageBind;
	std::unordered_map<std::string, size_t> shaderBind;
	std::vector<ImageResource*> swcRelativeImages;

	const ImageResource* displayImageRes;
	const Image* displayImage;
	Renderer* renderer;
	ExecutionGraph execGraph;
};
