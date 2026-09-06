#include "Renderer.hpp"
#pragma comment(lib,"C:\\VulkanSDK\\1.4.304.1\\Lib\\vulkan-1.lib")
#define STRINGIFY_(x) #x
#define EXIT_ON_VK_ERROR(expr){VkResult __result__ = (expr); if(__result__ != VK_SUCCESS){\
	MessageBox(NULL, L"vkResult is error\nLINE: " STRINGIFY_(__LINE__) "\nFILE: " STRINGIFY_(__FILE__), NULL, MB_OK); exit(-1); }}

constexpr static uint32_t Q_GRAPHICS = (uint32_t)QueueType::Graphics;
constexpr static uint32_t Q_COMPUTE= (uint32_t)QueueType::Compute;
constexpr static uint32_t Q_PRES = (uint32_t)QueueType::Presentation;


const static char* instExt[] = {
	VK_KHR_SURFACE_EXTENSION_NAME,
	VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
	VK_KHR_WIN32_SURFACE_EXTENSION_NAME};

const static char* devExt[] = { 
	VK_KHR_SWAPCHAIN_EXTENSION_NAME,
	VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME };

const static char* vaLayers[] = { 
	"VK_LAYER_KHRONOS_validation" };

using namespace std;


inline static VkDeviceQueueCreateInfo CreateQueueInfo(
	uint32_t queueFamilyIndex,
	uint32_t count,
	float* priority)
{
	VkDeviceQueueCreateInfo queueInfo = {};
	queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queueInfo.queueFamilyIndex = queueFamilyIndex;
	queueInfo.queueCount = count;
	queueInfo.pQueuePriorities = priority;
	return queueInfo;
}

Renderer::Renderer(
	HINSTANCE hinstance, 
	HWND hwnd)
	:
	windowHwnd(hwnd), swc({})
{
	InitVulkan();
	CreateSurface(hinstance, hwnd);
	PickPhysicalDevice();
	CreateLogicalDevice();
	CreateSwapchain();
}

void Renderer::OnResize(
	HWND hwnd, 
	void* renderer)
{
	((Renderer*)renderer)->OnResize(hwnd);
}

void Renderer::InitVulkan()
{
	uint32_t propCount;
	EXIT_ON_VK_ERROR(vkEnumerateInstanceExtensionProperties(nullptr, &propCount, nullptr));
	vector<VkExtensionProperties> instExtSupported(propCount);
	EXIT_ON_VK_ERROR(vkEnumerateInstanceExtensionProperties(nullptr, &propCount, instExtSupported.data()));
	uint32_t elemCount = sizeof(instExt) / sizeof(const char*);
	if (!CheckSupportForExt(instExt, elemCount, instExtSupported.data(), (uint32_t)instExtSupported.size()))
	{
		MessageBox(NULL, L"\nUnsupported extension found! \n", NULL, MB_OK);
		exit(-1);
	}

	VkDebugUtilsMessengerCreateInfoEXT debugInfo = {};
	debugInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	debugInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	debugInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	debugInfo.pfnUserCallback = VbDebugVal;

	VkApplicationInfo appInfo;
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "Sponza";
	appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.pEngineName = "Simple Renderer";
	appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.apiVersion = VK_API_VERSION_1_4;
	appInfo.pNext = NULL;

	VkInstanceCreateInfo instanceInfo = {};
	instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	instanceInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugInfo;;
	instanceInfo.enabledExtensionCount = sizeof(instExt) / sizeof(const char*);
	instanceInfo.ppEnabledExtensionNames = instExt;
	instanceInfo.enabledLayerCount = sizeof(vaLayers) / sizeof(const char*);
	instanceInfo.ppEnabledLayerNames = vaLayers;
	instanceInfo.pApplicationInfo = &appInfo;

	EXIT_ON_VK_ERROR(vkCreateInstance(&instanceInfo, nullptr, &instance));
}

bool Renderer::CheckSupportForExt(
	const char** requiredExtensions, 
	uint32_t requiredExtensionsNum, 
	const VkExtensionProperties* supportedExtensions, 
	uint32_t supportedExtensionsNum)
{
	uint32_t requiredExtensionsCount = 0;
	uint8_t* requiredExtFlags = new uint8_t[requiredExtensionsNum];
	memset(requiredExtFlags, 0, sizeof(uint8_t) * requiredExtensionsNum);

	for (uint32_t i = 0; i < supportedExtensionsNum; i++)
	{
		const char* extName = supportedExtensions[i].extensionName;
		for (uint32_t j = 0; j < requiredExtensionsNum; j++)
		{
			if (requiredExtFlags[j] != 1 && strcmp(extName, requiredExtensions[j]) == 0)
			{
				requiredExtFlags[j] = 1;
				requiredExtensionsCount++;
				break;
			}
		}

		if (requiredExtensionsCount == requiredExtensionsNum)
		{
			delete[] requiredExtFlags;
			return true;
		}
	}
	delete[] requiredExtFlags;
	return false;
}

void Renderer::CreateQueueIndecies()
{
	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(phDev, &queueFamilyCount, nullptr);
	std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(phDev, &queueFamilyCount, queueFamilies.data());

	queueIdx[0] = -1;
	queueIdx[1] = -1;
	queueIdx[2] = -1;

	for (int64_t i = 0; i < (int64_t)queueFamilies.size(); i++)
	{
		if (queueIdx[Q_GRAPHICS] == -1 && (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) > 0)
		{
			queueIdx[Q_GRAPHICS] = i;
		}

		if (queueIdx[Q_COMPUTE] == -1 && (queueFamilies[i].queueFlags & VK_QUEUE_COMPUTE_BIT) > 0)
		{
			queueIdx[Q_COMPUTE] = i;
		}

		VkBool32 surfaceSupport;
		EXIT_ON_VK_ERROR(vkGetPhysicalDeviceSurfaceSupportKHR(phDev, (uint32_t)i, surface, &surfaceSupport));
		if (queueIdx[Q_PRES] == -1 && surfaceSupport)
		{
			queueIdx[Q_PRES] = i;
		}

		if (queueIdx[0] != -1 && queueIdx[1] != -1 && queueIdx[2] != -1)
		{
			break;
		}
	}

	if (queueIdx[0] == -1 || queueIdx[1] == -1 || queueIdx[2] == -1)
	{
		MessageBox(NULL, L"Required queue is not supported in the system \n", NULL, MB_OK);
		exit(-1);
	}
}

void Renderer::CreateSurface(
	HINSTANCE hinstance, 
	HWND hwnd)
{
	surface = VK_NULL_HANDLE;

	VkWin32SurfaceCreateInfoKHR surfInfo = {};
	surfInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
	surfInfo.hinstance = hinstance;
	surfInfo.hwnd = hwnd;

	EXIT_ON_VK_ERROR(vkCreateWin32SurfaceKHR(instance, &surfInfo, nullptr, &surface));
}

VkBool32 Renderer::VbDebugVal(
	VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT messageTypes,
	const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
	void* pUserData)
{
	if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
	{
		OutputDebugStringA("validation layer: ");
		OutputDebugStringA(pCallbackData->pMessage);
		OutputDebugStringA("\n");
	}
	return VK_FALSE;
}

void Renderer::PickPhysicalDevice()
{
	uint32_t count;
	EXIT_ON_VK_ERROR(vkEnumeratePhysicalDevices(instance, &count, nullptr));
	vector<VkPhysicalDevice> physicalDevices(count, VK_NULL_HANDLE);
	EXIT_ON_VK_ERROR(vkEnumeratePhysicalDevices(instance, &count, physicalDevices.data()));

	for (VkPhysicalDevice dev : physicalDevices)
	{
		VkPhysicalDeviceProperties props = {};
		VkPhysicalDeviceFeatures features = {};
		vkGetPhysicalDeviceProperties(dev, &props);
		vkGetPhysicalDeviceFeatures(dev, &features);

		if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU && 
			features.shaderFloat64 && 
			features.samplerAnisotropy &&
			features.fillModeNonSolid)
		{
			phDev = dev;
			return;
		}
	}
	MessageBox(NULL, L"\nDiscrete GPU that support requested features does not exist! \n", NULL, MB_OK);
	exit(-1);
}

void Renderer::CreateLogicalDevice()
{
	CreateQueueIndecies();

	uint32_t propCount;
	EXIT_ON_VK_ERROR(vkEnumerateDeviceExtensionProperties(phDev, nullptr, &propCount, nullptr));
	vector<VkExtensionProperties> devExtSupported(propCount);
	EXIT_ON_VK_ERROR(vkEnumerateDeviceExtensionProperties(phDev, nullptr, &propCount, devExtSupported.data()));
	uint32_t extCount = sizeof(devExt) / sizeof(const char*);
	if (!CheckSupportForExt(devExt, extCount, devExtSupported.data(), (uint32_t)devExtSupported.size()))
	{
		MessageBox(NULL, L"\nUnsupported extension found! \n", NULL, MB_OK);
		exit(-1);
	}

	float priority[3] = { 1.0f, 1.0f, 1.0f };
	uint32_t infoCount = 1;
	VkDeviceQueueCreateInfo queueInfo[3] = {};
	queueInfo[Q_GRAPHICS] = CreateQueueInfo((uint32_t)queueIdx[Q_GRAPHICS], 1, priority);
	if (queueIdx[Q_COMPUTE] != queueIdx[Q_GRAPHICS])
	{
		queueInfo[infoCount] = CreateQueueInfo((uint32_t)queueIdx[Q_COMPUTE], 1, priority);
		infoCount++;
	}
	else
	{
		queueInfo[Q_GRAPHICS].queueCount++;
	}

	if (queueIdx[Q_PRES] != queueIdx[Q_GRAPHICS] && queueIdx[Q_PRES] != queueIdx[Q_COMPUTE])
	{
		queueInfo[infoCount] = CreateQueueInfo((uint32_t)queueIdx[Q_PRES], 1, priority);
		infoCount++;
	}
	else
	{
		queueIdx[Q_PRES] == queueIdx[Q_GRAPHICS] ? 
			queueInfo[Q_GRAPHICS].queueCount++ : queueInfo[Q_COMPUTE].queueCount++;
	}

	VkPhysicalDeviceFeatures features = {};
	features.samplerAnisotropy = VK_TRUE;
	features.fillModeNonSolid = VK_TRUE;

	VkDeviceCreateInfo devInfo = {};
	devInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	devInfo.queueCreateInfoCount = infoCount;
	devInfo.pQueueCreateInfos = queueInfo;
	devInfo.ppEnabledLayerNames = vaLayers;
	devInfo.enabledExtensionCount = extCount;
	devInfo.ppEnabledExtensionNames = devExt;
	devInfo.pEnabledFeatures = &features;

	EXIT_ON_VK_ERROR(vkCreateDevice(phDev, &devInfo, nullptr, &lgDev));
}

void Renderer::querySwapChainSupport()
{
	EXIT_ON_VK_ERROR(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(phDev, surface, &swc.capabilities));

	uint32_t formatCount;
	EXIT_ON_VK_ERROR(vkGetPhysicalDeviceSurfaceFormatsKHR(phDev, surface, &formatCount, nullptr));
	if (formatCount != 0)
	{
		swc.formats.resize(formatCount);
		EXIT_ON_VK_ERROR(vkGetPhysicalDeviceSurfaceFormatsKHR(phDev, surface, &formatCount, swc.formats.data()));
	}

	uint32_t presentModeCount;
	EXIT_ON_VK_ERROR(vkGetPhysicalDeviceSurfacePresentModesKHR(phDev, surface, &presentModeCount, nullptr));
	if (presentModeCount != 0)
	{
		swc.presentModes.resize(presentModeCount);
		EXIT_ON_VK_ERROR(vkGetPhysicalDeviceSurfacePresentModesKHR(phDev, surface, &presentModeCount, swc.presentModes.data()));
	}

}

void Renderer::CreateSwapchain()
{
	querySwapChainSupport();

	size_t i;
	for (i = 0; i < swc.formats.size(); i++)
	{
		if (swc.formats[i].format == GetSwapchainFormat())
		{
			break;
		}
	}
	if (i == swc.formats.size())
	{
		MessageBox(NULL, L"Unsupported VK_FORMAT_R8G8B8A8_UNORM! \n", NULL, MB_OK);
		exit(-1);
	}


	uint32_t imgCount = swc.capabilities.minImageCount + 1 <= swc.capabilities.maxImageCount ?
		swc.capabilities.minImageCount + 1 :
		swc.capabilities.minImageCount;
	VkSwapchainKHR swapchain = VK_NULL_HANDLE;
	VkSwapchainCreateInfoKHR info = {};
	info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	info.surface = surface;
	info.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
	info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	info.imageArrayLayers = 1;
	info.imageExtent = swc.capabilities.currentExtent;
	info.minImageCount = imgCount;
	info.preTransform = swc.capabilities.currentTransform;
	info.imageFormat = swc.formats[i].format;
	info.imageColorSpace = swc.formats[i].colorSpace;
	info.clipped = VK_FALSE;
	info.oldSwapchain = VK_NULL_HANDLE;

	uint32_t familyCount = 1;
	uint32_t queueFamilies[3] = { (uint32_t)queueIdx[Q_GRAPHICS], 0, 0 };

	if (queueIdx[Q_COMPUTE] != queueIdx[Q_GRAPHICS])
	{
		queueFamilies[familyCount] = (uint32_t)queueIdx[Q_COMPUTE];
		familyCount++;
	}
	if (queueIdx[Q_PRES] != queueIdx[Q_GRAPHICS] && queueIdx[Q_PRES] != queueIdx[Q_COMPUTE])
	{
		queueFamilies[familyCount] = (uint32_t)queueIdx[Q_PRES];
		familyCount++;
	}

	if (familyCount > 1)
	{
		info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		info.queueFamilyIndexCount = familyCount;
		info.pQueueFamilyIndices = queueFamilies;
	}
	else
	{
		info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		info.queueFamilyIndexCount = 0;
		info.pQueueFamilyIndices = nullptr; 
	}

	EXIT_ON_VK_ERROR(vkCreateSwapchainKHR(lgDev, &info, nullptr, &swapchain));

	uint32_t imageCount;
	EXIT_ON_VK_ERROR(vkGetSwapchainImagesKHR(lgDev, swapchain, &imageCount, nullptr));
	swc.images.resize(imageCount);
	EXIT_ON_VK_ERROR(vkGetSwapchainImagesKHR(lgDev, swapchain, &imageCount, swc.images.data()));

	swc.views.resize(swc.images.size());
	for (size_t i = 0; i < swc.images.size(); i++)
	{
		VkImageViewCreateInfo imgInfo = {};
		imgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		imgInfo.image = swc.images[i];
		imgInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		imgInfo.format = GetSwapchainFormat();
		imgInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		imgInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		imgInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		imgInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		imgInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imgInfo.subresourceRange.baseMipLevel = 0;
		imgInfo.subresourceRange.levelCount = 1;
		imgInfo.subresourceRange.baseArrayLayer = 0;
		imgInfo.subresourceRange.layerCount = 1;

		EXIT_ON_VK_ERROR(vkCreateImageView(lgDev, &imgInfo, nullptr, &swc.views[i]));
	}


}

void Renderer::OnResize(HWND hwnd)
{
}
