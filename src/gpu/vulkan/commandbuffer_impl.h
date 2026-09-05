#pragma once

#include <gallium/gpu/commandbuffer.h>
#include "device_impl.h"
#include "graphicspipeline_impl.h"
#include "computepipeline_impl.h"
#include "raytracingpipeline_impl.h"

namespace ga::gpu
{
	struct CommandBuffer::Impl
	{
		vk::raii::CommandBuffer commandBuffer  = nullptr;
		vk::raii::Semaphore     semaphore      = nullptr;
		vk::raii::Fence         isGpuFree      = nullptr;
		std::atomic_bool        isCpuFree      = false;
		bool                    isAsyncCompute = false;
	};

	struct CommandEncoder::Impl
	{
		CommandBuffer*           owner;
		vk::raii::CommandBuffer* commandBuffer;
		vk::DescriptorSet        bindlessDescriptorSet;
	};

	struct RenderEncoder::Impl
	{
		const CommandEncoder*           parent;
		vk::raii::CommandBuffer*        commandBuffer;
		const vk::raii::PipelineLayout* pipelineLayout;
	};

	struct ComputeEncoder::Impl
	{
		const CommandEncoder*           parent;
		vk::raii::CommandBuffer*        commandBuffer;
		const vk::raii::PipelineLayout* pipelineLayout;
	};

	struct RaytracingEncoder::Impl
	{
		const CommandEncoder*              parent;
		vk::raii::CommandBuffer*           commandBuffer;
		const vk::raii::PipelineLayout*    pipelineLayout;
		const RaytracingPipeline::SBTInfo* sbtInfo;
	};

	struct TransferEncoder::Impl
	{
		const CommandEncoder*    parent;
		vk::raii::CommandBuffer* commandBuffer;
	};

	struct TransferEncoder::BufferCopyInfoInternal
	{
		vk::Buffer     source;
		vk::Buffer     destination;
		vk::BufferCopy region;
	};
}
