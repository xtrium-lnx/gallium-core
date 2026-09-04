#include "commandbuffer_impl.h"
#include "buffer_impl.h"
#include "image_impl.h"
#include "descriptorregistry_impl.h"
#include "enums_impl.h"
#include "shadermodule_reflection_impl.h"

#include <glm/gtc/type_ptr.hpp>

#include <ranges>

using namespace ga::gpu;

CommandBuffer::CommandBuffer(const Device& owner, bool isAsyncCompute /* = false */)
	: m_pImpl(new Impl)
	, m_owner(owner)
{
	vk::CommandBufferAllocateInfo allocInfo {
		.commandPool        = isAsyncCompute ? owner.GetImpl().asyncComputeCommandPool : owner.GetImpl().commandPool,
		.level              = vk::CommandBufferLevel::ePrimary,
		.commandBufferCount = 1
	};

	m_pImpl->commandBuffer  = std::move(vk::raii::CommandBuffers(owner.GetImpl().device, allocInfo).front());
	m_pImpl->semaphore      = vk::raii::Semaphore(owner.GetImpl().device, vk::SemaphoreCreateInfo {});
	m_pImpl->isGpuFree      = vk::raii::Fence(owner.GetImpl().device, vk::FenceCreateInfo { .flags = vk::FenceCreateFlagBits::eSignaled });
	m_pImpl->isCpuFree      = true;
	m_pImpl->isAsyncCompute = isAsyncCompute;
}

CommandBuffer::~CommandBuffer()
{
	m_pImpl->isGpuFree.clear();
	m_pImpl->semaphore.clear();
	m_pImpl->commandBuffer.clear();
}

CommandBuffer::Impl& CommandBuffer::GetImpl() const
{
	return *m_pImpl;
}

const Device& CommandBuffer::GetOwner() const
{
	return m_owner;
}

bool CommandBuffer::IsCpuFree() const
{
	return m_pImpl->isCpuFree;
}

bool CommandBuffer::IsGpuFree() const
{
	return m_pImpl->isGpuFree.getStatus() == vk::Result::eSuccess;
}

void CommandBuffer::WaitForCompletion()
{
	auto fence = (VkFence)*m_pImpl->isGpuFree;
	vkWaitForFences(m_pImpl->isGpuFree.getDevice(), 1, &fence, true, UINT64_MAX);
}

void CommandBuffer::Record(std::function<void(const CommandEncoder&)> contents, ECommandBufferUsage flags /* = ECommandBufferUsage::None */)
{
	m_pImpl->commandBuffer.begin({
		.flags = ((flags & ECommandBufferUsage::OneTimeSubmit) != ECommandBufferUsage::None)
			? vk::CommandBufferUsageFlagBits::eOneTimeSubmit
			: vk::CommandBufferUsageFlags(0)
	});

	contents(CommandEncoder(*this));

	m_pImpl->commandBuffer.end();
}

// ----------------------------------------------------------------------------

CommandEncoder::CommandEncoder(CommandBuffer& cb)
	: m_pImpl(new Impl)
{
	m_pImpl->commandBuffer         = &cb.GetImpl().commandBuffer;
	m_pImpl->bindlessDescriptorSet = *const_cast<Device&>(cb.GetOwner()).GetDescriptorRegistry().GetImpl().set;
}

const CommandEncoder::Impl& CommandEncoder::GetImpl() const
{
	return *m_pImpl;
}

void CommandEncoder::PushLabel(const std::string& label, const glm::vec4& color /* = glm::vec4(1.0f) */) const
{
	auto labelInfo = vk::DebugUtilsLabelEXT {
		.pLabelName = label.c_str(),
		.color      = {{ color.r, color.g, color.b, color.a }}
	};
	m_pImpl->commandBuffer->beginDebugUtilsLabelEXT(labelInfo);
}

void CommandEncoder::PopLabel(size_t count /* = 1 */) const
{
	for (size_t i = 0; i < count; ++i)
		m_pImpl->commandBuffer->endDebugUtilsLabelEXT();
}

void CommandEncoder::Transition(Image& image, const TransitionInfo& info) const
{
	auto newLayout = s_ToVk(info.dstLayout);

	// FIXME: Maybe concurrency issues when working with the same image in multiple command buffers ?
	if (image.GetImpl().currentLayout == newLayout && !info.enforceBarrier)
		return;

	bool isDepth = (image.GetImpl().aspectMask & vk::ImageAspectFlagBits::eDepth) != vk::ImageAspectFlagBits::eNone;

	auto barrier = vk::ImageMemoryBarrier2 {
		.srcStageMask        = s_ToVk(info.srcStage),
		.srcAccessMask       = s_ToVk(info.srcAccess),
		.dstStageMask        = s_ToVk(info.dstStage),
		.dstAccessMask       = s_ToVk(info.dstAccess),
		.oldLayout           = image.GetImpl().currentLayout,
		.newLayout           = newLayout,
		.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.image = image.GetImpl().imageRaw,
		.subresourceRange = {
			.aspectMask     = isDepth ? vk::ImageAspectFlagBits::eDepth : vk::ImageAspectFlagBits::eColor,
			.baseMipLevel   = 0,
			.levelCount     = 1, // FIXME: Needs to work for multiple mip levels too
			.baseArrayLayer = 0,
			.layerCount     = 1  // FIXME: Needs to work for array images too
		}
	};

	auto dependencyInfo = vk::DependencyInfo {
		.dependencyFlags         = {},
		.imageMemoryBarrierCount = 1,
		.pImageMemoryBarriers    = &barrier
	};

	m_pImpl->commandBuffer->pipelineBarrier2(dependencyInfo);
	image.GetImpl().currentLayout = newLayout;
}

void CommandEncoder::Transition(Image& image, EImageLayout layout) const
{
	Transition(image, TransitionInfo {
		.srcStage  = EPipelineStage::TopOfPipe,
		.srcAccess = EAccessType::None,
		.dstStage  = EPipelineStage::ColorAttachmentOutput,
		.dstAccess = EAccessType::ColorAttachmentWrite,
		.dstLayout = layout
	});
}

void CommandEncoder::ResetQueryPool(QueryPoolId id) const
{
	m_pImpl->commandBuffer->resetQueryPool((VkQueryPool)id, 0, 2);
}

void CommandEncoder::WriteTimestamp(QueryPoolId id, EPipelineStage stage, uint32_t index) const
{
	m_pImpl->commandBuffer->writeTimestamp2(s_ToVk(stage), (VkQueryPool)id, index);
}

void CommandEncoder::Render(const BeginRenderingInfo& info, std::function<void(const RenderEncoder&)> contents) const
{
	contents(RenderEncoder(*this, info));
}

void CommandEncoder::Compute(const BeginComputeInfo& info, std::function<void(const ComputeEncoder&)> contents) const
{
	contents(ComputeEncoder(*this, info));
}

void CommandEncoder::Raytrace(const BeginRayTracingInfo& info, std::function<void(const RaytracingEncoder&)> contents) const
{
	contents(RaytracingEncoder(*this, info));
}

void CommandEncoder::Transfer(std::function<void(const TransferEncoder&)> contents) const
{
	contents(TransferEncoder(*this));
}

// ----------------------------------------------------------------------------

RenderEncoder::RenderEncoder(const CommandEncoder& parent, const BeginRenderingInfo& info)
	: m_pImpl(new Impl)
{
	m_pImpl->parent = &parent;
	m_pImpl->commandBuffer = parent.GetImpl().commandBuffer;

	auto colorAttachments = info.colorAttachments | std::views::transform([&info](const BeginRenderingInfo::ColorAttachment& a) {
		return vk::RenderingAttachmentInfo {
			.imageView   = *a.image.GetImpl().imageView,
			.imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
			.loadOp      = s_ToVk(a.loadOp),
			.storeOp     = s_ToVk(a.storeOp),
			.clearValue  = vk::ClearColorValue(a.clearValue.r, a.clearValue.g, a.clearValue.b, a.clearValue.a)
		};
	}) | std::ranges::to<std::vector>();

	auto depthAttachment = ([](const std::optional<BeginRenderingInfo::DepthAttachment>& a) -> std::optional<vk::RenderingAttachmentInfo> {
		if (!a)
			return std::nullopt;

		return vk::RenderingAttachmentInfo {
			.imageView   = *a->image.GetImpl().imageView,
			.imageLayout = vk::ImageLayout::eDepthAttachmentOptimal,
			.loadOp      = s_ToVk(a->loadOp),
			.storeOp     = s_ToVk(a->storeOp),
			.clearValue  = vk::ClearDepthStencilValue(a->clearValue)
		};
	})(info.depthAttachment);

	auto renderingInfo = vk::RenderingInfo {
		.renderArea = {
			.offset = { info.renderArea.offset.x, info.renderArea.offset.y },
			.extent = { info.renderArea.extent.x, info.renderArea.extent.y }
		},
		.layerCount           = info.layerCount,
		.colorAttachmentCount = uint32_t(colorAttachments.size()),
		.pColorAttachments    = colorAttachments.empty() ? nullptr : colorAttachments.data(),
		.pDepthAttachment     = depthAttachment ? &*depthAttachment : nullptr
	};

	m_pImpl->commandBuffer->beginRendering(renderingInfo);
	m_pImpl->commandBuffer->bindPipeline(vk::PipelineBindPoint::eGraphics, *info.pipeline.GetImpl().pipeline);
	m_pImpl->commandBuffer->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *info.pipeline.GetImpl().pipelineLayout, 0, { parent.GetImpl().bindlessDescriptorSet }, {});
	m_pImpl->pipelineLayout = &info.pipeline.GetImpl().pipelineLayout;
}

RenderEncoder::~RenderEncoder()
{
	m_pImpl->commandBuffer->endRendering();
}

const CommandEncoder& RenderEncoder::Parent() const
{
	return *m_pImpl->parent;
}

void RenderEncoder::BindVertexBuffers(uint32_t firstBinding, const std::vector<std::pair<Buffer*, uint64_t /* offset */>>& buffers) const
{
	auto bufs = buffers | std::views::transform([](const auto& b) { return b.first ? *b.first->GetImpl().buffer : VK_NULL_HANDLE; }) | std::ranges::to<std::vector>();
	auto offs = buffers | std::views::transform([](const auto& b) { return b.second; }) | std::ranges::to<std::vector>();
	m_pImpl->commandBuffer->bindVertexBuffers(firstBinding, bufs, offs);
}

void RenderEncoder::BindIndexBuffer(Buffer* buffer, uint64_t offset /* = 0 */) const
{
	m_pImpl->commandBuffer->bindIndexBuffer(*buffer->GetImpl().buffer, offset, vk::IndexType::eUint32);
}

void RenderEncoder::PushConstants(uint32_t offset, uint32_t size, const void* pData) const
{
	const std::byte* pbData = reinterpret_cast<const std::byte*>(pData);
	std::span<const std::byte> bytes(pbData, pbData + size);
	m_pImpl->commandBuffer->pushConstants<std::byte>(**m_pImpl->pipelineLayout, vk::ShaderStageFlagBits::eAll, offset, bytes);
}

void RenderEncoder::PushConstants(const ShaderStructInstance& data) const
{
	PushConstants(0, data.Size(), data.Data());
}

void RenderEncoder::Draw(uint32_t vertexCount, uint32_t instanceCount /* = 1 */, uint32_t firstVertex /* = 0 */, uint32_t firstInstance /* = 0 */) const
{
	m_pImpl->commandBuffer->draw(vertexCount, instanceCount, firstVertex, firstInstance);
}

void RenderEncoder::DrawIndexed(uint32_t indexCount, uint32_t instanceCount /* = 1 */, uint32_t firstIndex /* = 0 */, int32_t vertexOffset /* = 0 */, uint32_t firstInstance /* = 0 */) const
{
	m_pImpl->commandBuffer->drawIndexed(indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
}

void RenderEncoder::DrawIndirect(Buffer& buffer) const
{
	m_pImpl->commandBuffer->drawIndirect(*buffer.GetImpl().buffer, 0u, 1, sizeof(vk::DrawIndirectCommand));
}

void RenderEncoder::SetViewport(const glm::vec2& origin, const glm::vec2& size, const glm::vec2& zBounds /* = { 0.0f, 1.0f } */) const
{
	m_pImpl->commandBuffer->setViewport(0, {{
		origin.x, origin.y + size.y,
		size.x, -size.y,
		zBounds.x, zBounds.y
	}});
}

void RenderEncoder::SetScissor(const glm::ivec2& offset, const glm::uvec2& extent) const
{
	// Enforce compliance to VUID-vkCmdSetScissor-x-00595
	int ox = glm::max(0, offset.x);
	int oy = glm::max(0, offset.y);

	m_pImpl->commandBuffer->setScissor(0, {{{ ox, oy }, { extent.x, extent.y }}});
}

// ----------------------------------------------------------------------------

ComputeEncoder::ComputeEncoder(const CommandEncoder& parent, const BeginComputeInfo& info)
	: m_pImpl(new Impl)
{
	m_pImpl->parent = &parent;
	m_pImpl->commandBuffer = parent.GetImpl().commandBuffer;

	m_pImpl->commandBuffer->bindPipeline(vk::PipelineBindPoint::eCompute, *info.pipeline.GetImpl().pipeline);
	m_pImpl->commandBuffer->bindDescriptorSets(vk::PipelineBindPoint::eCompute, *info.pipeline.GetImpl().pipelineLayout, 0, { parent.GetImpl().bindlessDescriptorSet }, {});
	m_pImpl->pipelineLayout = &info.pipeline.GetImpl().pipelineLayout;
}

ComputeEncoder::~ComputeEncoder()
{
}

const CommandEncoder& ComputeEncoder::Parent() const
{
	return *m_pImpl->parent;
}

void ComputeEncoder::PushConstants(uint32_t offset, uint32_t size, const void* pData) const
{
	const std::byte* pbData = reinterpret_cast<const std::byte*>(pData);
	std::span<const std::byte> bytes(pbData, pbData + size);
	m_pImpl->commandBuffer->pushConstants<std::byte>(**m_pImpl->pipelineLayout, vk::ShaderStageFlagBits::eAll, offset, bytes);
}

void ComputeEncoder::PushConstants(const ShaderStructInstance& data) const
{
	PushConstants(0, data.Size(), data.Data());
}

void ComputeEncoder::Dispatch(const glm::uvec3& workgroups) const
{
	m_pImpl->commandBuffer->dispatch(workgroups.x, workgroups.y, workgroups.z);
}

void ComputeEncoder::DispatchIndirect(const Buffer& indirectBuffer) const
{
	m_pImpl->commandBuffer->dispatchIndirect(*indirectBuffer.GetImpl().buffer, 0u);
}

// ----------------------------------------------------------------------------

RaytracingEncoder::RaytracingEncoder(const CommandEncoder& parent, const BeginRayTracingInfo& info)
	: m_pImpl(new Impl)
{
	m_pImpl->parent = &parent;
	m_pImpl->commandBuffer = parent.GetImpl().commandBuffer;

	m_pImpl->commandBuffer->bindPipeline(vk::PipelineBindPoint::eRayTracingKHR, *info.pipeline.GetImpl().pipeline);
	m_pImpl->commandBuffer->bindDescriptorSets(vk::PipelineBindPoint::eRayTracingKHR, *info.pipeline.GetImpl().pipelineLayout, 0, { parent.GetImpl().bindlessDescriptorSet }, {});
	m_pImpl->pipelineLayout = &info.pipeline.GetImpl().pipelineLayout;
	m_pImpl->sbtInfo        = &info.pipeline.GetSBTInfo();
}

RaytracingEncoder::~RaytracingEncoder()
{
}

const CommandEncoder& RaytracingEncoder::Parent() const
{
	return *m_pImpl->parent;
}

void RaytracingEncoder::PushConstants(uint32_t offset, uint32_t size, const void* pData) const
{
	const std::byte* pbData = reinterpret_cast<const std::byte*>(pData);
	std::span<const std::byte> bytes(pbData, pbData + size);
	m_pImpl->commandBuffer->pushConstants<std::byte>(**m_pImpl->pipelineLayout, vk::ShaderStageFlagBits::eAll, offset, bytes);
}

void RaytracingEncoder::PushConstants(const ShaderStructInstance& data) const
{
	PushConstants(0, data.Size(), data.Data());
}

void RaytracingEncoder::TraceRays(const glm::uvec3& size) const
{
	auto toVk = [](const RaytracingPipeline::SBTTable& t) {
        return vk::StridedDeviceAddressRegionKHR {
            .deviceAddress = t.baseAddress,
            .stride        = t.recordStride,
            .size          = t.totalSize,
        };
    };

    auto raygen   = toVk(m_pImpl->sbtInfo->raygen);
    auto miss     = toVk(m_pImpl->sbtInfo->miss);
    auto hit      = toVk(m_pImpl->sbtInfo->hit);
    auto callable = toVk(m_pImpl->sbtInfo->callable);

    m_pImpl->commandBuffer->traceRaysKHR(raygen, miss, hit, callable, size.x, size.y, size.z);
}

// ----------------------------------------------------------------------------

TransferEncoder::TransferEncoder(const CommandEncoder& parent)
	: m_pImpl(new Impl)
{
	m_pImpl->parent = &parent;
	m_pImpl->commandBuffer = parent.GetImpl().commandBuffer;
}

TransferEncoder::~TransferEncoder()
{
}

const CommandEncoder& TransferEncoder::Parent() const
{
	return *m_pImpl->parent;
}

void TransferEncoder::ClearColorImage(Image& image, const glm::vec4& color) const
{
	auto clearRange = vk::ImageSubresourceRange {
		.aspectMask = vk::ImageAspectFlagBits::eColor,
		.levelCount = vk::RemainingMipLevels,
		.layerCount = vk::RemainingArrayLayers
	};

	m_pImpl->commandBuffer->clearColorImage(image.GetImpl().imageRaw, image.GetImpl().currentLayout, vk::ClearColorValue(color.r, color.g, color.b, color.a), clearRange);
}

void TransferEncoder::CopyImage(Image& src, Image& dst, const glm::uvec3& srcOffset, const glm::uvec3& dstOffset, const glm::uvec3& extent) const
{
	vk::ImageCopy copyRegion {
		.srcSubresource = { src.GetImpl().aspectMask, 0, 0, 1 },
		.srcOffset = { int(srcOffset.x), int(srcOffset.y), int(srcOffset.z) },
		.dstSubresource = { dst.GetImpl().aspectMask, 0, 0, 1 },
		.dstOffset = { int(dstOffset.x), int(dstOffset.y), int(dstOffset.z) },
		.extent = vk::Extent3D { extent.x, extent.y, extent.z }
	};

	m_pImpl->commandBuffer->copyImage(
		src.GetImpl().imageRaw, src.GetImpl().currentLayout,
		dst.GetImpl().imageRaw, dst.GetImpl().currentLayout,
		copyRegion
	);
}

void TransferEncoder::BlitImage(Image& src, Image& dst, const glm::uvec3& srcOffset, const glm::uvec3& dstOffset, const glm::uvec3& extent) const
{
	const std::array srcOffsets = {
		vk::Offset3D { int(srcOffset.x), int(srcOffset.y), int(srcOffset.z) },
		vk::Offset3D { int(srcOffset.x + extent.x), int(srcOffset.y + extent.y), int(srcOffset.z + extent.z) }
	};

	const std::array dstOffsets = {
		vk::Offset3D { int(dstOffset.x), int(dstOffset.y), int(dstOffset.z) },
		vk::Offset3D { int(dstOffset.x + extent.x), int(dstOffset.y + extent.y), int(dstOffset.z + extent.z) }
	};

	vk::ImageBlit blitRegion{
		.srcSubresource = { vk::ImageAspectFlagBits::eColor, 0, 0, 1 },
		.srcOffsets = srcOffsets,
		.dstSubresource = { vk::ImageAspectFlagBits::eColor, 0, 0, 1 },
		.dstOffsets = dstOffsets
	};

	m_pImpl->commandBuffer->blitImage(
		src.GetImpl().imageRaw, src.GetImpl().currentLayout,
		dst.GetImpl().imageRaw, dst.GetImpl().currentLayout,
		blitRegion, vk::Filter::eNearest
	);
}

void TransferEncoder::FillBuffer(Buffer& buffer, size_t offset, size_t size, uint32_t data) const
{
	m_pImpl->commandBuffer->fillBuffer(*buffer.GetImpl().buffer, offset, size, data);
}

void TransferEncoder::CopyBuffer(const BufferCopyInfoInternal& copyInfo) const
{
	m_pImpl->commandBuffer->copyBuffer(copyInfo.source, copyInfo.destination, { copyInfo.region });
}

void TransferEncoder::CopyBuffer(Buffer* source, Buffer* destination, size_t sourceOffset, size_t destinationOffset, size_t size) const
{
	CopyBuffer({
		*source->GetImpl().buffer,
		*destination->GetImpl().buffer,
		{ sourceOffset, destinationOffset, size }
	});
}

void TransferEncoder::CopyBufferToImage(Buffer* source, Image* destination, size_t bufferOffset, const glm::uvec3& imageOffset, const glm::uvec3& imageExtent) const
{
	CopyBufferToImageEx(source, destination, bufferOffset, 0, imageOffset, imageExtent);
}

void TransferEncoder::CopyBufferToImageEx(Buffer* source, Image* destination, size_t bufferOffset, uint32_t baseMip, const glm::uvec3& imageOffset, const glm::uvec3& imageExtent) const
{
	auto copyRegion = vk::BufferImageCopy {
		.bufferOffset      = bufferOffset,
		.bufferRowLength   = 0,
		.bufferImageHeight = 0,
		.imageSubresource = {
			.aspectMask     = vk::ImageAspectFlagBits::eColor,
			.mipLevel       = baseMip,
			.baseArrayLayer = 0,
			.layerCount     = 1,
		},
		.imageOffset = vk::Offset3D(imageOffset.x, imageOffset.y, imageOffset.z),
		.imageExtent = vk::Extent3D(imageExtent.x, imageExtent.y, imageExtent.z),
	};

	m_pImpl->commandBuffer->copyBufferToImage(source->GetImpl().buffer, destination->GetImpl().imageRaw, vk::ImageLayout::eTransferDstOptimal, copyRegion);
}

void TransferEncoder::CopyImageToBuffer(Image* source, Buffer* destination, const glm::uvec3& imageOffset, const glm::uvec3& imageExtent, uint32_t bufferOffset) const
{
	auto copyRegion = vk::BufferImageCopy {
		.bufferOffset      = bufferOffset,
		.bufferRowLength   = 0,
		.bufferImageHeight = 0,
		.imageSubresource = {
			.aspectMask     = vk::ImageAspectFlagBits::eColor,
			.mipLevel       = 0,
			.baseArrayLayer = 0,
			.layerCount     = 1,
		},
		.imageOffset = vk::Offset3D(imageOffset.x, imageOffset.y, imageOffset.z),
		.imageExtent = vk::Extent3D(imageExtent.x, imageExtent.y, imageExtent.z),
	};

	m_pImpl->commandBuffer->copyImageToBuffer(source->GetImpl().imageRaw, vk::ImageLayout::eTransferSrcOptimal, destination->GetImpl().buffer, copyRegion);
}

void TransferEncoder::BufferBarrier(const Buffer& buffer) const
{
	auto barrier = vk::BufferMemoryBarrier {
		.srcAccessMask       = vk::AccessFlagBits::eTransferWrite | vk::AccessFlagBits::eShaderWrite,
		.dstAccessMask       = vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eIndirectCommandRead | vk::AccessFlagBits::eVertexAttributeRead,
		.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.buffer              = *buffer.GetImpl().buffer,
		.offset              = 0,
		.size                = VK_WHOLE_SIZE,
	};

	m_pImpl->commandBuffer->pipelineBarrier(
		vk::PipelineStageFlagBits::eTransfer | vk::PipelineStageFlagBits::eComputeShader,
		vk::PipelineStageFlagBits::eAllCommands,
		vk::DependencyFlagBits(0),
		{}, { barrier }, {});
}

void TransferEncoder::ASBarrier() const
{
	auto barrier = vk::MemoryBarrier2 {
		.srcStageMask        = vk::PipelineStageFlagBits2::eAccelerationStructureBuildKHR,
		.srcAccessMask       = vk::AccessFlagBits2::eAccelerationStructureWriteKHR,
		.dstStageMask        = vk::PipelineStageFlagBits2::eRayTracingShaderKHR | vk::PipelineStageFlagBits2::eComputeShader | vk::PipelineStageFlagBits2::eFragmentShader,
		.dstAccessMask       = vk::AccessFlagBits2::eAccelerationStructureReadKHR
	};

	auto dep = vk::DependencyInfo {
		.memoryBarrierCount = 1,
		.pMemoryBarriers    = &barrier,
	};

	m_pImpl->commandBuffer->pipelineBarrier2(dep);
}
