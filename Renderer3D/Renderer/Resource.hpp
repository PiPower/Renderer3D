#pragma once
#include <vulkan/vulkan.h>
#include <stdint.h>
#include <vector>

enum class BindLevel
{
	NONE = 0,
	PER_PASS,
	PER_MATERIAL,
	PER_OBJECT
};

constexpr uint32_t SWAPCHAIN_RELATIVE = 0xffffffff;
// struct that describes a usage of a buffer resource in a render graph.
struct BufferResource
{
	uint8_t isDefined : 1 = 0;
	VkBufferUsageFlags usage = 0;
	VkDeviceSize size = 0;
	bool isHostVisible;
};

// struct that describes a usage of a image resource in a render graph.
struct ImageResource
{;
	uint8_t isDefined : 1 = 0;
	uint32_t width = 0;
	uint32_t height = 0;
	uint32_t layers = 0;
	VkFormat format = VK_FORMAT_UNDEFINED;
	VkSampleCountFlagBits samples = (VkSampleCountFlagBits)0;
	VkImageViewType viewType = (VkImageViewType)0;
	VkImageLayout inStageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	VkImageUsageFlags aux_usage = 0;
};