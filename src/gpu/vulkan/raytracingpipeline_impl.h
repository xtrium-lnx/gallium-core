#ifndef GALLIUM__GPU__VULKAN__RAYTRACINGPIPELINE_H
#define GALLIUM__GPU__VULKAN__RAYTRACINGPIPELINE_H
#pragma once

#include <gallium/gpu/raytracingpipeline.h>
#include <gallium/gpu/buffer.h>

#include "device_impl.h"

namespace ga::gpu
{
	struct RaytracingPipeline::Impl
	{
		vk::raii::PipelineLayout    pipelineLayout = nullptr;
		vk::raii::Pipeline          pipeline       = nullptr;

		std::unique_ptr<Buffer>     sbtBuffer;
		RaytracingPipeline::SBTInfo sbtInfo = {};
	};
}

#endif /* GALLIUM__GPU__VULKAN__RAYTRACINGPIPELINE_H */
