#pragma once

#include <Windows.h>
#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.hpp>
#include <inttypes.h>
#include "RenderUtils.hpp"

class Renderer
{
public:
	Renderer(
		HINSTANCE hinstance,
		HWND hwnd);

	static void OnResize(
		HWND hwnd,
		void* renderer);
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

	void OnResize(HWND hwnd);

private:
	HWND windowHwnd;
	VkInstance instance;
	VkPhysicalDevice phDev;
	VkSurfaceKHR surface;
	VkDevice lgDev;
	int64_t queueIdx[3];
	VkQueue queues[3];
};

