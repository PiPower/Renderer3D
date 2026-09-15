#pragma once
#include <vulkan/vulkan.h>
#include <unordered_map>
#include "RenderPass.hpp"
#include "Renderer.hpp"

struct ExecutionGraph
{
	std::vector<VkPipeline> pipelines;
	std::vector<Image> imageResources;
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
};

class RenderGraph
{
public:
	RenderGraph(
		const std::vector<std::string>& bufferNames,
		const std::vector<std::string>& imageNames,
		const std::vector<ShaderDesc>& shaderDescs);

	RenderPass* CreateRenderPass(
		const std::string& name,
		bool isGraphicsPass);

	void Compile(Renderer* renderer);

	inline ExecutionGraph* GetExecutionGraph() { return &execGraph; }

	ImageResource* QueryImage(const std::string& name);

	BufferResource* QueryBuffer(const std::string& name);

	ShaderDesc* QueryShader(const std::string& name);

	void DescribeVertexBuffer(
		const std::string& name,
		uint32_t stride,
		const std::vector<VkFormat>& vertexInputFormats,
		const std::vector<uint32_t>& formatOffsets);

	void DescribeImage(
		const std::string& name,
		uint32_t width,
		uint32_t height,
		uint32_t layers,
		VkFormat format,
		VkSampleCountFlagBits samples,
		VkImageViewType viewType);

private:
	VkPipeline CompilePipeline(
		Renderer* renderer, 
		RenderPass* renderPass);

	std::vector<VkPipelineShaderStageCreateInfo> CompileShaders(
		Renderer* renderer,
		RenderPass* renderPass);

	PipelineInputDesc CreatePipelineInput(RenderPass* renderPass);

	PipelineRenderingDesc CreatePipelineRendering(RenderPass* renderPass);

	void AllocateResources(Renderer* renderer);
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

	ExecutionGraph execGraph;
};
