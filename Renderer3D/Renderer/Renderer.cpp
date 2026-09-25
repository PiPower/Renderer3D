#include "Renderer.hpp"
//#pragma comment(lib,"C:\\VulkanSDK\\1.4.304.1\\Lib\\vulkan-1.lib")
#pragma comment(lib, "vulkan-1.lib")
#undef min
#define STRINGIFY_(x) #x
#define STRINGIFY(x) STRINGIFY_(x)
#define EXIT_ON_VK_ERROR(expr){VkResult __result__ = (expr); if(__result__ != VK_SUCCESS){\
	MessageBox(NULL, L"vkResult is error\nLINE: " STRINGIFY(__LINE__) "\nFILE: " STRINGIFY(__FILE__), NULL, MB_OK); exit(-1); }}

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
	HWND hwnd,
	uint64_t stagingSize)
	:
	windowHwnd(hwnd), swc({})
{
	InitVulkan();
	CreateSurface(hinstance, hwnd);
	PickPhysicalDevice();
	CreateLogicalDevice();

	vkGetDeviceQueue(lgDev, (uint32_t)queueIdx[Q_GRAPHICS], 0, &queues[Q_GRAPHICS]);
	vkGetDeviceQueue(lgDev, (uint32_t)queueIdx[Q_COMPUTE], 1, &queues[Q_COMPUTE]);
	vkGetDeviceQueue(lgDev, (uint32_t)queueIdx[Q_PRES], 0, &queues[Q_PRES]);

	CreateSwapchain();
	CreateCommandStructs();
	PrepareRenderingResources();
	CreateSynchPrim();

	VkBufferCreateInfo buffInfo = {};
	buffInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	buffInfo.size = stagingSize;
	buffInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	buffInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	stagingBuffer = AllocateBuffer(buffInfo, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
}

void Renderer::OnResize(
	HWND hwnd, 
	void* renderer)
{
	((Renderer*)renderer)->OnResize(hwnd);
}

void Renderer::RenderFrame()
{
	//EXIT_ON_VK_ERROR(vkWaitForFences(lgDev, 1, &gfxQueueFinished, VK_TRUE, UINT64_MAX));
	//EXIT_ON_VK_ERROR(vkResetFences(lgDev, 1, &gfxQueueFinished));
	EXIT_ON_VK_ERROR(vkAcquireNextImageKHR(lgDev, swc.swapchain, UINT64_MAX, imgReady, VK_NULL_HANDLE, &imageIndex));


	VkPresentInfoKHR info = {};
	info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	info.swapchainCount = 1;
	info.pSwapchains = &swc.swapchain;
	info.pImageIndices = &imageIndex;
	info.waitSemaphoreCount = 1;
	info.pWaitSemaphores = &imgReady;
	EXIT_ON_VK_ERROR(vkQueuePresentKHR(queues[Q_PRES], &info));
	vkQueueWaitIdle(queues[Q_PRES]);
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

Image Renderer::AllocateImage(
	const VkImageCreateInfo& imgInfo,
	const VkImageViewCreateInfo& viewInfo,
	VkMemoryPropertyFlagBits memProps)
{
	Image out = {};
	VkMemoryRequirements memoryRequirements;
	VkImageViewCreateInfo info = viewInfo;

	EXIT_ON_VK_ERROR(vkCreateImage(lgDev, &imgInfo, nullptr, &out.img));
	vkGetImageMemoryRequirements(lgDev, out.img, &memoryRequirements);
	out.mem = AllocateMemory(memProps, memoryRequirements);
	EXIT_ON_VK_ERROR(vkBindImageMemory(lgDev, out.img, out.mem, 0));
	info.image = out.img;
	EXIT_ON_VK_ERROR(vkCreateImageView(lgDev, &info, nullptr, &out.imgView));
	out.range = viewInfo.subresourceRange;
	out.currLayout = imgInfo.initialLayout;
	return out;
}

Buffer Renderer::AllocateBuffer(
	const VkBufferCreateInfo& buffInfo,
	VkMemoryPropertyFlagBits memProps)
{
	Buffer out = {};
	out.memProps = memProps;
	out.buffInfo = buffInfo;
	VkMemoryRequirements memoryRequirements = {};

	EXIT_ON_VK_ERROR(vkCreateBuffer(lgDev, &out.buffInfo, nullptr, &out.buff));
	vkGetBufferMemoryRequirements(lgDev, out.buff, &memoryRequirements);
	out.mem = AllocateMemory(memProps, memoryRequirements);
	EXIT_ON_VK_ERROR(vkBindBufferMemory(lgDev, out.buff, out.mem, 0));

	if ( (memProps & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) > 0)
	{
		EXIT_ON_VK_ERROR(vkMapMemory(lgDev, out.mem, 0, out.buffInfo.size, 0, (void**) & out.mmap));
	}
	return out;
}

void Renderer::UploadStagingToImage(
	Image* img,
	uint32_t regionCount, 
	const VkBufferImageCopy* pRegions)
{
	VkImageMemoryBarrier barriers[2];
	barriers[0] = {};
	barriers[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barriers[0].srcAccessMask = 0;
	barriers[0].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barriers[0].oldLayout = img->currLayout;
	barriers[0].newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barriers[0].image = img->img;
	barriers[0].subresourceRange = img->range;

	barriers[1] = {};
	barriers[1].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barriers[1].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barriers[1].dstAccessMask = 0;
	barriers[1].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barriers[1].newLayout = img->currLayout;
	barriers[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barriers[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barriers[1].image = img->img;
	barriers[1].subresourceRange = img->range;

	VkCommandBufferBeginInfo cmdInfo = { };
	cmdInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	cmdInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	EXIT_ON_VK_ERROR(vkResetCommandBuffer(gfxCmd, 0));
	EXIT_ON_VK_ERROR(vkBeginCommandBuffer(gfxCmd, &cmdInfo));

	vkCmdPipelineBarrier(gfxCmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
			VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, barriers);

	vkCmdCopyBufferToImage(gfxCmd, stagingBuffer.buff, img->img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, pRegions);

	vkCmdPipelineBarrier(gfxCmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, barriers + 1);

	EXIT_ON_VK_ERROR(vkEndCommandBuffer(gfxCmd));

	VkSubmitInfo submit = {};
	submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit.commandBufferCount = 1;
	submit.pCommandBuffers = &gfxCmd;
	EXIT_ON_VK_ERROR(vkQueueSubmit(queues[Q_GRAPHICS], 1, &submit, VK_NULL_HANDLE));
	EXIT_ON_VK_ERROR(vkQueueWaitIdle(queues[Q_GRAPHICS]));
}

void Renderer::UploadDataToBuffer(
	Buffer* dst,
	Buffer* src, 
	VkDeviceSize uploadSize,
	VkDeviceSize srcOffset,
	VkDeviceSize dstOffset)
{
	VkCommandBufferBeginInfo cmdInfo = { };
	cmdInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	cmdInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	EXIT_ON_VK_ERROR(vkResetCommandBuffer(gfxCmd, 0));
	EXIT_ON_VK_ERROR(vkBeginCommandBuffer(gfxCmd, &cmdInfo));
	
	VkBufferCopy copy = {};
	copy.dstOffset = 0;
	copy.srcOffset = 0;
	copy.size = uploadSize;

	vkCmdCopyBuffer(gfxCmd, src->buff, dst->buff, 1, &copy);

	EXIT_ON_VK_ERROR(vkEndCommandBuffer(gfxCmd));

	VkSubmitInfo submit = {};
	submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit.commandBufferCount = 1;
	submit.pCommandBuffers = &gfxCmd;
	EXIT_ON_VK_ERROR(vkQueueSubmit(queues[Q_GRAPHICS], 1, &submit, VK_NULL_HANDLE));
	EXIT_ON_VK_ERROR(vkQueueWaitIdle(queues[Q_GRAPHICS]));
}

void Renderer::UploadDataToBuffer(
	Buffer* dst,
	const char* src, 
	VkDeviceSize uploadSize,
	VkDeviceSize srcOffset,
	VkDeviceSize dstOffset)
{
	if (uploadSize > stagingBuffer.buffInfo.size)
	{
		MessageBox(NULL, L"requested upload size is larger than allocated staging buffer", NULL, MB_OK); 
		exit(-1);
	}
	VkDeviceSize sizeCorretion = (devLimits.nonCoherentAtomSize - uploadSize % devLimits.nonCoherentAtomSize) % 64;
	memcpy(stagingBuffer.mmap, src, uploadSize);
	VkMappedMemoryRange range = {};
	range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
	range.memory = stagingBuffer.mem;
	range.offset = 0;
	range.size = std::min(uploadSize + sizeCorretion, stagingBuffer.buffInfo.size);
	EXIT_ON_VK_ERROR(vkFlushMappedMemoryRanges(lgDev, 1, &range));

	UploadDataToBuffer(dst, &stagingBuffer, uploadSize, srcOffset, dstOffset);
}

void Renderer::RunCommandsAndSync(const VkSubmitInfo& submitInfo)
{
	EXIT_ON_VK_ERROR(vkQueueSubmit(queues[Q_GRAPHICS], 1, &submitInfo, nullptr));
	EXIT_ON_VK_ERROR(vkQueueWaitIdle(queues[Q_GRAPHICS]));
}

void Renderer::DisplayImageAndSync(
	VkImage srcImage,
	VkImageLayout layout)
{
	EXIT_ON_VK_ERROR(vkAcquireNextImageKHR(lgDev, swc.swapchain, UINT64_MAX, imgReady, VK_NULL_HANDLE, &imageIndex));


	VkImageSubresourceRange subresourceRange = {};
	subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	subresourceRange.baseMipLevel = 0;
	subresourceRange.levelCount = 1;
	subresourceRange.baseArrayLayer = 0;
	subresourceRange.layerCount = 1;

	VkImageSubresourceLayers layers = {};
	layers.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	layers.mipLevel = 0;
	layers.baseArrayLayer = 0;
	layers.layerCount = 1;

	VkImageCopy copyRegion = {};
	copyRegion.srcSubresource = layers;
	copyRegion.dstSubresource = layers;
	copyRegion.srcOffset = { 0, 0, 0 };
	copyRegion.dstOffset = { 0, 0, 0 };
	copyRegion.extent = { swc.capabilities.currentExtent.width, swc.capabilities.currentExtent.height, 1 };

	VkImageMemoryBarrier barriers[4];
	barriers[0] = {};
	barriers[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barriers[0].srcAccessMask = 0;
	barriers[0].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barriers[0].oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	barriers[0].newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barriers[0].image = swc.images[imageIndex];
	barriers[0].subresourceRange = subresourceRange;
	
	barriers[1] = {};
	barriers[1].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barriers[1].srcAccessMask = 0;
	barriers[1].dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
	barriers[1].oldLayout = layout;
	barriers[1].newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	barriers[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barriers[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barriers[1].image = srcImage;
	barriers[1].subresourceRange = subresourceRange;


	barriers[2] = {};
	barriers[2].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barriers[2].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barriers[2].dstAccessMask = 0;
	barriers[2].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barriers[2].newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	barriers[2].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barriers[2].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barriers[2].image = swc.images[imageIndex];
	barriers[2].subresourceRange = subresourceRange;


	barriers[3] = {};
	barriers[3].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barriers[3].srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
	barriers[3].dstAccessMask = 0;
	barriers[3].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	barriers[3].newLayout = layout;
	barriers[3].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barriers[3].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barriers[3].image = srcImage;
	barriers[3].subresourceRange = subresourceRange;

	VkCommandBufferBeginInfo cmdBuffInfo = {};
	cmdBuffInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	cmdBuffInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	EXIT_ON_VK_ERROR(vkResetCommandBuffer(gfxCmd, 0));
	EXIT_ON_VK_ERROR(vkBeginCommandBuffer(gfxCmd, &cmdBuffInfo));

	vkCmdPipelineBarrier(gfxCmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
		VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 2, barriers);

	vkCmdCopyImage(gfxCmd, srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		swc.images[imageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1, &copyRegion);

	vkCmdPipelineBarrier(gfxCmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
		VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 2, barriers + 2);

	EXIT_ON_VK_ERROR(vkEndCommandBuffer(gfxCmd));
	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &gfxCmd;
	EXIT_ON_VK_ERROR(vkQueueSubmit(queues[Q_GRAPHICS], 1, &submitInfo, nullptr));
	vkQueueWaitIdle(queues[Q_GRAPHICS]);

	VkPresentInfoKHR info = {};
	info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	info.swapchainCount = 1;
	info.pSwapchains = &swc.swapchain;
	info.pImageIndices = &imageIndex;
	info.waitSemaphoreCount = 1;
	info.pWaitSemaphores = &imgReady;
	EXIT_ON_VK_ERROR(vkQueuePresentKHR(queues[Q_PRES], &info));
	vkQueueWaitIdle(queues[Q_PRES]);

}

VkDescriptorSetLayout Renderer::CreateDescriptorSet(const VkDescriptorSetLayoutCreateInfo* info)
{
	VkDescriptorSetLayout setLayout;
	EXIT_ON_VK_ERROR(vkCreateDescriptorSetLayout(lgDev, info, nullptr, &setLayout));
	return setLayout;
}

VkPipelineLayout Renderer::CreatePipelineLayout(
	const std::vector<VkDescriptorSetLayout>& sets,
	const std::vector<VkPushConstantRange>& pushConstants)
{
	VkPipelineLayout layout;
	VkPipelineLayoutCreateInfo layoutInfo = {};
	layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	layoutInfo.setLayoutCount = (uint32_t)sets.size();
	layoutInfo.pSetLayouts = sets.data();
	layoutInfo.pushConstantRangeCount = (uint32_t)pushConstants.size();
	layoutInfo.pPushConstantRanges = pushConstants.data();
	EXIT_ON_VK_ERROR(vkCreatePipelineLayout(lgDev, &layoutInfo, nullptr, &layout));
	
	return layout;
}

VkCommandPool Renderer::CreateGraphicsCommandPool()
{
	VkCommandPool cmdPool;
	VkCommandPoolCreateInfo poolInfo = {};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	poolInfo.queueFamilyIndex = GetGfxQueueIdx();
	EXIT_ON_VK_ERROR(vkCreateCommandPool(lgDev, &poolInfo, nullptr, &cmdPool));

	return cmdPool;
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
			devLimits = props.limits;
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

	VkPhysicalDeviceDynamicRenderingFeatures dynamicRendering{};
	dynamicRendering.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES;
	dynamicRendering.dynamicRendering = VK_TRUE;

	VkDeviceCreateInfo devInfo = {};
	devInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	devInfo.pNext = &dynamicRendering;
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

	EXIT_ON_VK_ERROR(vkCreateSwapchainKHR(lgDev, &info, nullptr, &swc.swapchain));

	uint32_t imageCount;
	EXIT_ON_VK_ERROR(vkGetSwapchainImagesKHR(lgDev, swc.swapchain, &imageCount, nullptr));
	swc.images.resize(imageCount);
	EXIT_ON_VK_ERROR(vkGetSwapchainImagesKHR(lgDev, swc.swapchain, &imageCount, swc.images.data()));

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

void Renderer::CreateCommandStructs()
{
	VkCommandPoolCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	info.queueFamilyIndex = (uint32_t)queueIdx[Q_GRAPHICS];

	EXIT_ON_VK_ERROR(vkCreateCommandPool(lgDev, &info, nullptr, &gfxPool));

	VkCommandBufferAllocateInfo infoCommand = {};
	infoCommand.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	infoCommand.commandBufferCount = 1;
	infoCommand.commandPool = gfxPool;
	infoCommand.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

	EXIT_ON_VK_ERROR(vkAllocateCommandBuffers(lgDev, &infoCommand, &gfxCmd));
}

void Renderer::PrepareRenderingResources()
{
	vector<VkImageMemoryBarrier> barriers;
	barriers.resize(swc.images.size());
	for (size_t i = 0; i < swc.images.size(); i++)
	{
		barriers[i] = {};
		barriers[i].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barriers[i].srcAccessMask = 0;
		barriers[i].dstAccessMask = 0;
		barriers[i].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		barriers[i].newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		barriers[i].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barriers[i].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barriers[i].image = swc.images[i];
		barriers[i].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barriers[i].subresourceRange.baseMipLevel = 0;
		barriers[i].subresourceRange.levelCount = 1;
		barriers[i].subresourceRange.baseArrayLayer = 0;
		barriers[i].subresourceRange.layerCount = 1;
	}
	VkCommandBufferBeginInfo cmdBuffInfo = {};
	cmdBuffInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	cmdBuffInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	EXIT_ON_VK_ERROR(vkResetCommandBuffer(gfxCmd, 0));
	EXIT_ON_VK_ERROR(vkBeginCommandBuffer(gfxCmd, &cmdBuffInfo));

	vkCmdPipelineBarrier(gfxCmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
		VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, (uint32_t)barriers.size(), barriers.data());

	EXIT_ON_VK_ERROR(vkEndCommandBuffer(gfxCmd));
	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &gfxCmd;
	EXIT_ON_VK_ERROR(vkQueueSubmit(queues[Q_GRAPHICS], 1, &submitInfo, nullptr));
	EXIT_ON_VK_ERROR(vkQueueWaitIdle(queues[Q_GRAPHICS]));

}

void Renderer::OnResize(HWND hwnd)
{
}

void Renderer::CreateSynchPrim()
{
	VkSemaphoreCreateInfo semaphoreInfo{};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	EXIT_ON_VK_ERROR(vkCreateSemaphore(lgDev, &semaphoreInfo, nullptr, &imgReady));
	EXIT_ON_VK_ERROR(vkCreateSemaphore(lgDev, &semaphoreInfo, nullptr, &renderingFinished));

	VkFenceCreateInfo fenceInfo{};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
	EXIT_ON_VK_ERROR(vkCreateFence(lgDev, &fenceInfo, nullptr, &gfxQueueFinished));
}

VkDeviceMemory Renderer::AllocateMemory(
	VkMemoryPropertyFlagBits memProps,
	const VkMemoryRequirements& memReqs)
{
	VkDeviceMemory memOut;
	VkPhysicalDeviceMemoryProperties memProperties = {};
	vkGetPhysicalDeviceMemoryProperties(phDev, &memProperties);

	uint32_t idx;
	for (idx = 0; idx < memProperties.memoryTypeCount; idx++)
	{
		if ((memReqs.memoryTypeBits & (1 << idx) ) == 0)
		{
			continue;
		}

		if ((memProperties.memoryTypes[idx].propertyFlags & memProps) == memProps)
		{
			break;
		}
	}

	if (memProperties.memoryTypeCount == idx)
	{
		MessageBox(NULL, L"Could not find suitable memory heap", NULL, MB_OK);
		exit(-1);
	}

	VkMemoryAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memReqs.size;
	allocInfo.memoryTypeIndex = idx;
	EXIT_ON_VK_ERROR(vkAllocateMemory(lgDev, &allocInfo, nullptr, &memOut));
	return memOut;
}
