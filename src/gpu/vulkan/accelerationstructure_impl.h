#ifndef GALLIUM__GPU__VULKAN__ACCELERATIONSTRUCTURE_H
#define GALLIUM__GPU__VULKAN__ACCELERATIONSTRUCTURE_H
#pragma once

#include <gallium/gpu/accelerationstructure.h>
#include "device_impl.h"

struct ga::gpu::ASGeometry
{
	vk::raii::AccelerationStructureKHR blas           = nullptr;
	uintptr_t                          blasGpuAddress = 0Ui64;
	std::unique_ptr<Buffer>            blasBuffer     = nullptr;
};

struct ga::gpu::AccelerationStructure::Impl
{
	vk::raii::AccelerationStructureKHR tlas           = nullptr;
	std::unique_ptr<Buffer>            tlasBuffer     = nullptr;
	std::unique_ptr<Buffer>            instanceBuffer = nullptr;
	uintptr_t                          gpuAddress     = 0;
};

#endif /* GALLIUM__GPU__VULKAN__ACCELERATIONSTRUCTURE_H */
