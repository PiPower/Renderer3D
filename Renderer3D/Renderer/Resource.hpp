#pragma once
#include <vulkan/vulkan.h>
#include <stdint.h>

namespace Size
{
	enum Class
	{
		Absolute,
		SwapchainRelative,
		InputRelative
	};
}

// struct that describes a usage of a buffer resource in a render graph.
struct BufferResource
{
	Size::Class sizeClass = Size::Absolute;
	VkBufferUsageFlags usage = 0;
	VkDeviceSize size = 0;
	// optional used if this buffer is used as a vertex buffer
	std::vector<VkFormat> vertexInputFormats;
	std::vector<uint32_t> formatOffsets;
};

// struct that describes a usage of a image resource in a render graph.
struct ImageResource
{
	VkFormat format = VK_FORMAT_UNDEFINED;
	VkImageLayout processingLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	VkImageLayout outputLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	VkImageUsageFlags aux_usage = 0;
};
