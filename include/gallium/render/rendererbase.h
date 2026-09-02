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

	class RendererBase
	{
	public:
		virtual ~RendererBase() = default;

		virtual void AddGeometry(const MeshInstance& mesh, const glm::mat4& transform, uint32_t arrayIndex = 0) = 0;
		virtual void AddLight(const LightData& light, const glm::mat4& transform) = 0;
		virtual void SetEnvironment(uint32_t imageId, float intensity) = 0;
		virtual void SetCamera(const glm::mat4& view, const glm::mat4& projection) = 0;
		virtual bool HasCamera() const = 0;

		virtual ga::gpu::Image* RetrieveImage(string_hash_t id) { return nullptr; }

		virtual std::vector<gpu::SemaphoreId> Render(const ga::gpu::CommandEncoder& encoder, ga::gpu::Image& output) = 0;
		virtual void Reset() = 0;
	};
}

#endif /* GALLIUM__RENDER__RENDERERBASE_H */
