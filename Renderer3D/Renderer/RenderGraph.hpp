#pragma once
#include <vulkan/vulkan.h>
#include <unordered_map>
#include "RenderPass.hpp"
#include "Renderer.hpp"

struct ExecutionGraph
{
	std::vector<VkPipeline> pipelines;
};

struct ShaderDesc
{
	std::string name;
	std::string entryName;
	std::string path;
	VkShaderStageFlagBits stages;
	VkShaderModule bytecode;
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

	void AddShader(
		const std::string& name,
		const std::string& path,
		const std::string& entryName,
		VkShaderStageFlagBits shaderStage);

	void Compile(Renderer* renderer);

	VkPipeline CompilePipeline(
		Renderer* renderer, 
		RenderPass* renderPass);

	std::vector<VkPipelineShaderStageCreateInfo> CompileShaders(
		Renderer* renderer,
		RenderPass* renderPass);



	inline ExecutionGraph* GetExecutionGraph() const { return execGraph; }

	ImageResource* QueryImage(const std::string& name);

	BufferResource* QueryBuffer(const std::string& name);

	ShaderDesc* QueryShader(const std::string& name);

private:
	std::vector<RenderPass> renderPasses;
	std::vector<ImageResource> imgResource;
	std::vector<BufferResource> buffResource;
	std::vector<ShaderDesc> shaders;

	std::unordered_map<std::string, size_t> renderPassNames;
	std::unordered_map<std::string, size_t> bufferBind;
	std::unordered_map<std::string, size_t> imageBind;
	std::unordered_map<std::string, size_t> shaderBind;

	ExecutionGraph* execGraph;
};
