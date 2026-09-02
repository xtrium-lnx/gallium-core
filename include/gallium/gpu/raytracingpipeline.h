#ifndef GALLIUM__GPU__RAYTRACINGPIPELINE_H
#define GALLIUM__GPU__RAYTRACINGPIPELINE_H
#pragma once

#include <gallium/gpu/enums.h>
#include <gallium/gpu/pipeline.h>

#include <memory>
#include <optional>
#include <vector>

namespace ga::gpu
{
	class Device;

	enum class ERaytracingShaderGroupType
	{
		General,
		TrianglesHit,
		ProceduralHit,
	};

	struct RaytracingShaderGroupDesc
	{
		ERaytracingShaderGroupType type = ERaytracingShaderGroupType::General;
		uint32_t generalShader          = uint32_t(-1);
		uint32_t closestHitShader       = uint32_t(-1);
		uint32_t anyHitShader           = uint32_t(-1);
		uint32_t intersectionShader     = uint32_t(-1);
	};

	struct RaytracingPipelineInfo
	{
		std::vector<PipelineStageDesc>          stages;
		std::vector<RaytracingShaderGroupDesc>  shaderGroups;
		uint32_t                                maxRecursionDepth = 1;
	};

	class RaytracingPipeline
	{
		struct Impl;
		std::unique_ptr<Impl> m_pImpl;

	public:
		RaytracingPipeline(Device& device, const RaytracingPipelineInfo& info);
		~RaytracingPipeline();

		struct SBTTable
		{
			uint64_t baseAddress  = 0;
			uint64_t recordStride = 0;
			uint64_t totalSize    = 0;
		};

		struct SBTInfo
		{
			SBTTable raygen;
			SBTTable miss;
			SBTTable hit;
			SBTTable callable;
		};

		const SBTInfo& GetSBTInfo() const;
		const Impl&    GetImpl()    const;
	};
}

#endif /* GALLIUM__GPU__RAYTRACINGPIPELINE_H */
