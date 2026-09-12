#include "RenderPass.hpp"
#include "RenderGraph.hpp"
#include <stdexcept>

void RenderPass::AddTextureImage(const std::string& name)
{
}

void RenderPass::AddInputImage(const std::string& name)
{
}

void RenderPass::AddOutputImage(const std::string& name)
{
}

void RenderPass::AddUniformBuffer(const std::string& name)
{
	BufferResource* buf = rg->QueryBuffer(name);
	if (buf == nullptr)
	{
		throw std::runtime_error("Buffer with name '" + name + "' does not exist.");
	}

	buf->usage |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	buffers.push_back(buf);
}

void RenderPass::AddVertexBuffer(const std::string& name)
{
	BufferResource* buf = rg->QueryBuffer(name);
	if (buf == nullptr)
	{
		throw std::runtime_error("Buffer with name '" + name + "' does not exist.");
	}

	buf->usage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	vertexBuffers.push_back(buf);
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

