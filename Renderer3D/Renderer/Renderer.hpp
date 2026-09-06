#pragma once

#include <Windows.h>
#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.hpp>
#include <inttypes.h>

enum class QueueType
{
	Graphics = 0,
	Compute = 1,
	Presentation = 2
};

struct Swapchain
{
	VkSurfaceCapabilitiesKHR capabilities;
	std::vector<VkSurfaceFormatKHR> formats;
	std::vector<VkPresentModeKHR> presentModes;
	std::vector<VkImage> images;
	std::vector<VkImageView> views;
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

	inline VkFormat GetSwapchainFormat() const
	{
		return VK_FORMAT_R8G8B8A8_UNORM;
	}
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

	void OnResize(HWND hwnd);

private:
	HWND windowHwnd;
	VkInstance instance;
	VkPhysicalDevice phDev;
	VkSurfaceKHR surface;
	VkDevice lgDev;
	int64_t queueIdx[3];
	VkQueue queues[3];
	Swapchain swc;
};

