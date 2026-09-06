#include "Renderer.hpp"
#pragma comment(lib,"C:\\VulkanSDK\\1.4.304.1\\Lib\\vulkan-1.lib")
#define STRINGIFY_(x) #x
#define EXIT_ON_VK_ERROR(expr){VkResult __result__ = (expr); if(__result__ != VK_SUCCESS){\
	MessageBox(NULL, L"vkResult is error\nLINE: " STRINGIFY_(__LINE__) "\nFILE: " STRINGIFY_(__FILE__), NULL, MB_OK); exit(-1); }}

constexpr static uint32_t Q_GRAPHICS = 0;
constexpr static uint32_t Q_COMPUTE= 1;
constexpr static uint32_t Q_PRES = 2;


const static char* instExt[] = {
					 VK_KHR_SURFACE_EXTENSION_NAME,
					 VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
					 VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
					VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME};
const static char* devExt[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
const static char* vaLayers[] = { "VK_LAYER_KHRONOS_validation" };

using namespace std;

Renderer::Renderer(
	HINSTANCE hinstance, 
	HWND hwnd)
	:
	windowHwnd(hwnd)
{
	InitVulkan();
	createSurface(hinstance, hwnd);
	PickPhysicalDevice();
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
	if (!checkSupportForExt(instExt, elemCount, instExtSupported.data(), (uint32_t)instExtSupported.size()))
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
	debugInfo.pfnUserCallback = vbDebugVal;

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

bool Renderer::checkSupportForExt(
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

void Renderer::createQueueIndecies()
{
	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(phDev, &queueFamilyCount, nullptr);
	std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(phDev, &queueFamilyCount, queueFamilies.data());

	queueIdx[0] = -1;
	queueIdx[1] = -1;
	queueIdx[2] = -1;

	for (int64_t i = 0; i < queueFamilies.size(); i++)
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
		EXIT_ON_VK_ERROR(vkGetPhysicalDeviceSurfaceSupportKHR(phDev, i, surface, &surfaceSupport));
		if (queueIdx[Q_PRES] == -1 && surfaceSupport)
		{
			queueIdx[Q_PRES] = i;
		}

		if (queueIdx[0] != -1 && queueIdx[1] != -1 && queueIdx[2] != -1)
		{
			break;
		}
	}

	if (queueIdx[0] != -1 && queueIdx[1] != -1 && queueIdx[2] != -1)
	{
		MessageBox(NULL, L"Required queue is not supported in the system \n", NULL, MB_OK);
		exit(-1);
	}
}

void Renderer::createSurface(
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

VkBool32 Renderer::vbDebugVal(
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

		if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU && features.shaderFloat64 && features.samplerAnisotropy && features.fillModeNonSolid)
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


	VkDeviceQueueCreateInfo queueInfo[3] = {};
	queueInfo[Q_GRAPHICS].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;

	queueInfo[Q_COMPUTE].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;

	queueInfo[Q_PRES].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;

	VkDeviceCreateInfo devInfo = {};
	devInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;


	EXIT_ON_VK_ERROR(vkCreateDevice(phDev, nullptr, nullptr, &lgDev));
}

void Renderer::OnResize(HWND hwnd)
{
}
