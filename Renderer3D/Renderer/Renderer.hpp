#pragma once

#include <Windows.h>
#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>
#include <vector>
#include <inttypes.h>

enum class QueueType
{
	Graphics = 0,
	Compute = 1,
	Presentation = 2
};

struct Swapchain
{
	VkSwapchainKHR swapchain;
	VkSurfaceCapabilitiesKHR capabilities;
	std::vector<VkSurfaceFormatKHR> formats;
	std::vector<VkPresentModeKHR> presentModes;
	std::vector<VkImage> images;
	std::vector<VkImageView> views;
};


struct Image
{
	VkDeviceMemory mem;
	VkImage img;
	VkImageView imgView;
};

struct Buffer
{
	VkDeviceMemory mem;
	VkBuffer buff;
};

class Renderer
{
public:
	Renderer(
		HINSTANCE hinstance,
		HWND hwnd);

	static void OnResize(
		HWND hwnd,
		void* renderer);

	void RenderFrame();

	Image AllocateImage(
		const VkImageCreateInfo& imgInfo,
		const VkImageViewCreateInfo& viewInfo,
		VkMemoryPropertyFlagBits memProps);

	Buffer AllocateBuffer(
		const VkBufferCreateInfo& buffInfo,
		VkMemoryPropertyFlagBits memProps);

	VkDescriptorSetLayout CreateDescriptorSet(const VkDescriptorSetLayoutCreateInfo* info);

	VkPipelineLayout CreatePipelineLayout(const std::vector< VkDescriptorSetLayout>& sets);

	inline VkFormat GetSwapchainFormat() const { return VK_FORMAT_R8G8B8A8_UNORM; }

	inline VkDevice GetDevice() { return lgDev; }

	inline VkSurfaceCapabilitiesKHR GetSwapchainCapabilities() { return swc.capabilities; }
private:
	static VkBool32 VbDebugVal(
		VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageTypes,
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
		void* pUserData);

	void InitVulkan();

	bool CheckSupportForExt(
		const char** requiredExtensions,
		uint32_t requiredExtensionsNum,
		const VkExtensionProperties* supportedExtensions,
		uint32_t supportedExtensionsNum);

	void CreateQueueIndecies();

	void CreateSurface(
		HINSTANCE hinstance,
		HWND hwnd);

	void PickPhysicalDevice();

	void CreateLogicalDevice();

	void querySwapChainSupport();

	void CreateSwapchain();

	void CreateCommandStructs();

	void PrepareRenderingResources();

	void OnResize(HWND hwnd);
	
	void CreateSynchPrim();

	VkDeviceMemory AllocateMemory(
		VkMemoryPropertyFlagBits memProps,
		const VkMemoryRequirements& memReqs);

private:
	HWND windowHwnd;
	VkInstance instance;
	VkPhysicalDevice phDev;
	VkSurfaceKHR surface;
	VkDevice lgDev;
	int64_t queueIdx[3];
	VkQueue queues[3];
	Swapchain swc;
	VkCommandPool gfxPool;
	VkCommandBuffer gfxCmd;
	VkSemaphore imgReady;
	VkSemaphore renderingFinished;
	VkFence gfxQueueFinished;
	uint32_t imageIndex;
};

