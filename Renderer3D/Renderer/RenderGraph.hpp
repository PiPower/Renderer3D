#pragma once
#include <vulkan/vulkan.h>
#include <unordered_map>
#include "RenderPass.hpp"
#include "Renderer.hpp"

struct RenderingPipeline
{
	VkPipeline pipeline;
	VkPipelineLayout layout;
	std::vector<VkDescriptorSetLayout> sets;
};

struct ExecutionGraph
{
	std::vector<RenderingPipeline> pipelines;
	std::vector<Image> imageResources;
	std::vector<Buffer> bufferResources;

	// resources are copies of all above handles, they are to be fed to their render steps
	std::vector<RenderResources> renderResources;
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


	inline ExecutionGraph* GetExecutionGraph() { return &execGraph; }

	ImageResource* QueryImage(const std::string& name);

	BufferResource* QueryBuffer(const std::string& name);

	ShaderDesc* QueryShader(const std::string& name);

	void DescribeBuffer(
		const std::string& name,
		uint32_t size);

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

private:
	RenderingPipeline CompilePipeline(RenderPass* renderPass);

	std::vector<VkPipelineShaderStageCreateInfo> CompileShaders(RenderPass* renderPass);

	PipelineInputDesc CreatePipelineInput(RenderPass* renderPass);

	PipelineRenderingDesc CreatePipelineRendering(RenderPass* renderPass);

	std::vector<VkDescriptorSetLayout> CreateSets(RenderPass* renderPass);

	std::vector<VkDescriptorSetLayoutBinding> CreateBufferBindings(
		const std::vector<UniformBuffer>& uniformBuffers,
		BindLevel level);

	void AllocateResources();
private:
	std::vector<RenderPass> renderPasses;
	std::vector<ImageResource> imgResource;
	std::vector<BufferResource> buffResource;
	std::vector<ShaderDesc> shaders;

	std::unordered_map<std::string, size_t> renderPassNames;
	std::unordered_map<std::string, size_t> bufferBind;
	std::unordered_map<std::string, size_t> imageBind;
	std::unordered_map<std::string, size_t> shaderBind;
	std::vector<ImageResource*> swcRelativeImages;

	Renderer* renderer;
	ExecutionGraph execGraph;
};
