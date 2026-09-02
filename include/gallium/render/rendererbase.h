#ifndef GALLIUM__RENDER__RENDERERBASE_H
#define GALLIUM__RENDER__RENDERERBASE_H
#pragma once

#include <gallium/string_hash.h>
#include <gallium/gpu/device.h>

#include <glm/glm.hpp>

namespace ga::gpu { class CommandEncoder; class Image; }

namespace ga::render
{
	struct MeshInstance;
	class  Terrain;
	class  ParticleSystemRegistry;

	enum class ELightType
		: uint32_t
	{
		Directional,
		Point,
		Spot
	};

	struct LightData
	{
		ELightType type;
		glm::vec3  color;
		float      intensity;
		glm::vec2  spotAngles = { glm::radians(15.0f), glm::radians(30.0f) };
		float      radius = 0.0f;
	};

	struct SDFOpcode
	{
		glm::uvec4 opcode;
		glm::vec4  data[3];
	};

	struct SDFShape
	{
		glm::vec3            aabb[2];
		uint32_t             materialId;
		std::list<SDFOpcode> opcodes;
	};

	struct AtmosphereData
	{
		glm::vec3 planetCenter     = glm::vec3(0.0f);
		float     planetRadius     = 1.0f;

		float     atmosphereThickness = 1.094f;
		float     densityScale        = 1.0f;
		float     distanceScale       = 1.0f;

		glm::vec3 rayleighColor       = { 0.3f, 0.5f, 1.0f };
		float     rayleighStrength    = 1.0f;

		float     mieStrength         = 0.1f;
		float     mieAnisotropy       = 0.76f;
	};

	class RendererBase
	{
	protected:
		SDFShape  m_sdfShapeBuildCurrent;
		glm::mat4 m_sdfShapeTransformCurrent;

	public:
		virtual ~RendererBase() = default;

		virtual void AddGeometry(const MeshInstance& mesh, const glm::mat4& transform, uint32_t arrayIndex = 0) = 0;
		virtual void AddLight(const LightData& light, const glm::mat4& transform) = 0;
		virtual void AddSDFShape(const SDFShape& shape, const glm::mat4& transform) = 0;
		virtual void SetEnvironment(uint32_t imageId, float intensity) = 0;
		virtual void SetAtmosphere(const AtmosphereData& data) = 0;
		virtual void SetTerrain(Terrain& data) = 0;
		virtual void SetParticleSystemRegistry(ParticleSystemRegistry& data) = 0;
		virtual void SetCamera(const glm::mat4& view, const glm::mat4& projection) = 0;
		virtual bool HasCamera() const = 0;

		virtual ga::gpu::Image* RetrieveImage(string_hash_t id) { return nullptr; }

		virtual std::vector<gpu::SemaphoreId> Render(const ga::gpu::CommandEncoder& encoder, ga::gpu::Image& output) = 0;
		virtual void Reset() = 0;

		void BeginSDFShape(const glm::vec3& aabbMin, const glm::vec3& aabbMax, const glm::mat4& transform, uint32_t materialId)
		{
			if (!m_sdfShapeBuildCurrent.opcodes.empty())
				AddSDFShape(m_sdfShapeBuildCurrent, m_sdfShapeTransformCurrent);

			m_sdfShapeBuildCurrent.opcodes.clear();
			m_sdfShapeBuildCurrent.aabb[0] = aabbMin;
			m_sdfShapeBuildCurrent.aabb[1] = aabbMax;
			m_sdfShapeBuildCurrent.materialId = materialId;
			m_sdfShapeTransformCurrent = transform;
		}

		void AddSDFShapeOpcode(const SDFOpcode& opcode)
		{
			m_sdfShapeBuildCurrent.opcodes.push_front(opcode);
		}
	};
}

#endif /* GALLIUM__RENDER__RENDERERBASE_H */
