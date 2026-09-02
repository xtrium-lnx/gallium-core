#include "raytracingpipeline_impl.h"
#include "enums_impl.h"
#include "shadermodule_impl.h"

#include <ranges>
#include <cstring>

using namespace ga::gpu;

static vk::RayTracingShaderGroupTypeKHR s_ToVk(ERaytracingShaderGroupType type)
{
	switch (type)
	{
	case ERaytracingShaderGroupType::General:        return vk::RayTracingShaderGroupTypeKHR::eGeneral;
	case ERaytracingShaderGroupType::TrianglesHit:   return vk::RayTracingShaderGroupTypeKHR::eTrianglesHitGroup;
	case ERaytracingShaderGroupType::ProceduralHit:  return vk::RayTracingShaderGroupTypeKHR::eProceduralHitGroup;
	}
	return vk::RayTracingShaderGroupTypeKHR::eGeneral;
}

static uint32_t s_AlignUp(uint32_t value, uint32_t alignment)
{ // Rounds `value` up to the nearest multiple of `alignment`.
	return (value + alignment - 1) & ~(alignment - 1);
}

RaytracingPipeline::RaytracingPipeline(Device& device, const RaytracingPipelineInfo& info)
	: m_pImpl(new Impl)
{
	const auto& vkDevice = device.GetImpl().device;
	
	auto pipelineStages = info.stages | std::views::transform([](const PipelineStageDesc& s) {
		return vk::PipelineShaderStageCreateInfo {
			.stage  = s_ToVk(s.stage),
			.module = s.module.GetImpl().shaderModule,
			.pName  = s.entrypoint.data()
		};
	}) | std::ranges::to<std::vector>();

	auto shaderGroups = info.shaderGroups | std::views::transform([](const RaytracingShaderGroupDesc& g) {
		return vk::RayTracingShaderGroupCreateInfoKHR {
			.type               = s_ToVk(g.type),
			.generalShader      = g.generalShader,
			.closestHitShader   = g.closestHitShader,
			.anyHitShader       = g.anyHitShader,
			.intersectionShader = g.intersectionShader
		};
	}) | std::ranges::to<std::vector>();

	std::optional<vk::PushConstantRange> pushConstantRange = std::nullopt;

	for (const auto& s : info.stages)
	{
		if (auto pc = s.module.GetStruct("PushConstants"); pc)
		{
			pushConstantRange = vk::PushConstantRange {
				.stageFlags = vk::ShaderStageFlagBits::eAll,
				.offset     = 0u,
				.size       = pc->size
			};
			break;
		}
	}

	auto pipelineLayoutInfo = vk::PipelineLayoutCreateInfo {
		.setLayoutCount         = 1u,
		.pSetLayouts            = &*device.GetDescriptorRegistry().GetImpl().layout,
		.pushConstantRangeCount = pushConstantRange.has_value() ? 1u : 0u,
		.pPushConstantRanges    = pushConstantRange.has_value() ? &*pushConstantRange : nullptr
	};
	m_pImpl->pipelineLayout = vk::raii::PipelineLayout(vkDevice, pipelineLayoutInfo);

	auto pipelineInfo = vk::RayTracingPipelineCreateInfoKHR {
		.stageCount                   = uint32_t(pipelineStages.size()),
		.pStages                      = pipelineStages.data(),
		.groupCount                   = uint32_t(shaderGroups.size()),
		.pGroups                      = shaderGroups.data(),
		.maxPipelineRayRecursionDepth = info.maxRecursionDepth,
		.layout                       = m_pImpl->pipelineLayout,
	};

	m_pImpl->pipeline = vk::raii::Pipeline(vkDevice, nullptr, nullptr, pipelineInfo);

	// -----------------------------------------------------------------------
	// Shader binding table
	//
	// Layout (each row is aligned to shaderGroupBaseAlignment):
	//
	//   [ raygen  | miss ... | hit ...  | callable ... ]
	//
	// We categorise groups by the stage type of their generalShader (for
	// General groups) or by hit-group type, matching the order expected by
	// vkCmdTraceRaysKHR.
	// -----------------------------------------------------------------------

	const auto& rtProps = device.GetImpl().deviceCaps.rtPipelineProperties;
	const uint32_t handleSize = rtProps.shaderGroupHandleSize;
	const uint32_t handleAlignment = rtProps.shaderGroupHandleAlignment;
	const uint32_t baseAlignment = rtProps.shaderGroupBaseAlignment;
	const uint32_t handleSizeAligned = s_AlignUp(handleSize, handleAlignment);

	// Collect the raw opaque handles from the driver.
	const uint32_t groupCount = uint32_t(info.shaderGroups.size());
	const uint32_t totalHandles = groupCount * handleSize;
	auto rawHandles = m_pImpl->pipeline.getRayTracingShaderGroupHandlesKHR<uint8_t>(0, groupCount, totalHandles);

	// Partition group indices into the four SBT tables.
	// We rely on the caller having placed groups in the conventional order
	// (raygen first, then miss, then hit, then callable) and we identify the
	// boundary by inspecting stage types.
	auto raygenIndices = std::vector<uint32_t>{};
	auto missIndices = std::vector<uint32_t>{};
	auto hitIndices = std::vector<uint32_t>{};
	auto callableIndices = std::vector<uint32_t>{};

	for (uint32_t i = 0; i < groupCount; ++i)
	{
		const auto& g = info.shaderGroups[i];
		if (g.type != ERaytracingShaderGroupType::General)
		{
			hitIndices.push_back(i);
			continue;
		}

		// For General groups, look up the stage type.
		if (g.generalShader == ~0u)
			continue;

		switch (info.stages[g.generalShader].stage)
		{
		case EShaderStage::RayGen:   raygenIndices.push_back(i);   break;
		case EShaderStage::Miss:     missIndices.push_back(i);     break;
		case EShaderStage::Callable: callableIndices.push_back(i); break;
		default: break;
		}
	}

	// Compute per-table sizes (aligned to baseAlignment).
	auto tableSize = [&](const std::vector<uint32_t>& indices) -> vk::DeviceSize {
		if (indices.empty()) return 0;
		return s_AlignUp(uint32_t(indices.size()) * handleSizeAligned, baseAlignment);
		};

	const vk::DeviceSize raygenSize = tableSize(raygenIndices);
	const vk::DeviceSize missSize = tableSize(missIndices);
	const vk::DeviceSize hitSize = tableSize(hitIndices);
	const vk::DeviceSize callableSize = tableSize(callableIndices);
	const vk::DeviceSize totalSize = raygenSize + missSize + hitSize + callableSize;

	// Pack handles into a CPU-side staging vector respecting alignment, then
	// hand it off to Buffer which handles allocation and upload.
	auto sbtData = std::vector<uint8_t>(totalSize, 0u);

	auto writeHandles = [&](size_t dstOffset, const std::vector<uint32_t>& indices)
		{
			for (uint32_t local = 0; local < uint32_t(indices.size()); ++local)
			{
				std::memcpy(
					sbtData.data() + dstOffset + local * handleSizeAligned,
					rawHandles.data() + indices[local] * handleSize,
					handleSize);
			}
		};

	size_t offset = 0;
	writeHandles(offset, raygenIndices);   offset += raygenSize;
	writeHandles(offset, missIndices);     offset += missSize;
	writeHandles(offset, hitIndices);      offset += hitSize;
	writeHandles(offset, callableIndices);

	m_pImpl->sbtBuffer = std::make_unique<Buffer>(device, BufferInfo{
		.usage = EBufferUsage::ShaderBindingTable | EBufferUsage::ShaderDeviceAddress,
		.size = totalSize,
		.initialData = sbtData.data(),
		});
	m_pImpl->sbtBuffer->SetDebugName("SBT");

	// Build the strided address regions from the buffer's GPU address.
	const uint64_t bufferAddress = m_pImpl->sbtBuffer->GetGpuAddress();

	// Convert to our backend-agnostic SBTTable at the Vulkan boundary.
	auto makeRegion = [&](uint64_t base, vk::DeviceSize size, uint32_t count) -> SBTTable {
		if (count == 0) return {};
		return {
			.baseAddress = base,
			.recordStride = uint64_t(handleSizeAligned),
			.totalSize = uint64_t(size),
		};
		};

	uint64_t addr = bufferAddress;
	m_pImpl->sbtInfo.raygen = { .baseAddress = addr, .recordStride = handleSizeAligned, .totalSize = handleSizeAligned };   addr += raygenSize;
	m_pImpl->sbtInfo.miss = makeRegion(addr, missSize, uint32_t(missIndices.size()));     addr += missSize;
	m_pImpl->sbtInfo.hit = makeRegion(addr, hitSize, uint32_t(hitIndices.size()));      addr += hitSize;
	m_pImpl->sbtInfo.callable = makeRegion(addr, callableSize, uint32_t(callableIndices.size()));
}

RaytracingPipeline::~RaytracingPipeline()
{
	m_pImpl->sbtBuffer.reset();
	m_pImpl->pipeline.clear();
	m_pImpl->pipelineLayout.clear();
}

const RaytracingPipeline::SBTInfo& RaytracingPipeline::GetSBTInfo() const
{
	return m_pImpl->sbtInfo;
}

const RaytracingPipeline::Impl& RaytracingPipeline::GetImpl() const
{
	return *m_pImpl;
}