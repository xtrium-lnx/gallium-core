#ifndef GALLIUM__GPU__VULKAN__DEVICE_H
#define GALLIUM__GPU__VULKAN__DEVICE_H
#pragma once

#include <gallium/gpu/device.h>

#define NOMINMAX
#define VK_USE_PLATFORM_WIN32_KHR
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>
#include <vma/vk_mem_alloc.h>

#include "../../platform/win32/platform_impl.h"
#include "commandbuffer_impl.h"
#include "descriptorregistry_impl.h"
#include "shadercache_impl.h"

#include <functional>
#include <memory>
#include <mutex>
#include <unordered_map>

struct GLFWwindow;

namespace ga::gpu
{
	class Image;

	struct FrameData
	{
		vk::raii::Semaphore                          presentCompleteSemaphore    = nullptr;
		vk::raii::Semaphore                          renderFinishedSemaphore     = nullptr;
		vk::raii::Fence                              inFlightFence               = nullptr;
		vk::raii::QueryPool                          queryPool                   = nullptr;
		std::vector<std::move_only_function<void()>> deferredActions;
	};

	struct Device::Impl
	{
		GLFWwindow*                         window;
		vk::raii::Instance                  instance                 = nullptr;
		vk::raii::PhysicalDevice            physicalDevice           = nullptr;
		vk::raii::SurfaceKHR                surface                  = nullptr;
		vk::raii::Device                    device                   = nullptr;

		bool                                supportsRaytracing       = false;

		vk::raii::SwapchainKHR              swapchain                = nullptr;
		std::vector<std::unique_ptr<Image>> swapchainImages;
		bool                                swapchainNeedsRecreation = false;

		VmaAllocator                        allocator                = VK_NULL_HANDLE;
		vk::raii::DescriptorPool            descriptorPool           = nullptr;
		std::unique_ptr<DescriptorRegistry> descriptorRegistry       = nullptr;

		uint32_t                                                           mainQueueFamily = uint32_t(-1);
		vk::raii::Queue                                                    mainQueue       = nullptr;
		vk::raii::CommandPool                                              commandPool     = nullptr;
		std::unordered_map<CommandBuffer*, std::unique_ptr<CommandBuffer>> commandBuffers;
		std::mutex                                                         commandBufferMutex;

		uint32_t                                                           asyncComputeQueueFamily = uint32_t(-1);
		vk::raii::Queue                                                    asyncComputeQueue       = nullptr;
		vk::raii::CommandPool                                              asyncComputeCommandPool = nullptr;
		std::unordered_map<CommandBuffer*, std::unique_ptr<CommandBuffer>> asyncComputeCommandBuffers;
		std::mutex                                                         asyncComputeCommandBufferMutex;

		std::vector<FrameData>              frameData;
		size_t                              currentFrame       = 0;
		uint32_t                            currentImageIndex  = 0;

		std::unique_ptr<ShaderCache>        shaderCache = nullptr;

		struct {
			vk::PhysicalDeviceRayTracingPipelinePropertiesKHR    rtPipelineProperties;
			vk::PhysicalDeviceAccelerationStructurePropertiesKHR asProperties;
		} deviceCaps;


		using CreateSwapchainReturnType = std::pair<vk::raii::SwapchainKHR, std::vector<std::unique_ptr<Image>>>;

		vk::raii::Instance        InitInstance(const char* appName, const glm::uvec3& appVersion);
		vk::raii::PhysicalDevice  PickPhysicalDevice(const DeviceCaps& caps);
		uint32_t                  PickQueueFamily(const std::function<bool(vk::QueueFlags)>& pred);
		vk::raii::SurfaceKHR      CreateGlfwWindowSurface(GLFWwindow* window);
		vk::raii::Device          CreateDevice(const std::vector<uint32_t>& queueFamilies, const DeviceCaps& caps);
		CreateSwapchainReturnType CreateSwapchain(const Device& owner, GLFWwindow* window);
		void                      RecreateSwapchain(const Device& owner, GLFWwindow* window);
		vk::raii::DescriptorPool  CreateDescriptorPool();
	};
}

ga::gpu::SemaphoreId s_FromVk(VkSemaphore semaphore);
VkSemaphore s_ToVk(ga::gpu::SemaphoreId id);

#endif /* GALLIUM__GPU__VULKAN__DEVICE_H */
