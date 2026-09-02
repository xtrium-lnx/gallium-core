#ifndef GALLIUM__GPU__ACCELERATIONSTRUCTURE_H
#define GALLIUM__GPU__ACCELERATIONSTRUCTURE_H
#pragma once

#include <functional>
#include <memory>
#include <vector>

#include <gallium/gpu/enums.h>
#include <glm/glm.hpp>

namespace ga::gpu
{
	class Device;
	class Buffer;
	class CommandEncoder;

	struct ASGeometry;

	struct ASGeometryInfo
	{
		EFormat vertexFormat = EFormat::R32G32B32_SFloat;
		Buffer* vertexBuffer;
		Buffer* indexBuffer;
		size_t  firstVertex;
		size_t  firstIndex;
		size_t  indexCount;
	};

	struct ASGeometryInstance
	{
		ASGeometry* geometry;
		glm::mat4   transform;
		uint32_t    customValue = 0;
		uint8_t     mask        = 0xff;
		uint32_t    hitGroup    = 0;
	};

	using ASProceduralInjector     = std::function<void(Buffer&, size_t, const CommandEncoder&)>;
	using ASProceduralInjectorList = std::vector<std::pair<size_t, ASProceduralInjector>>;

	class AccelerationStructure
	{
		struct Impl;
		std::unique_ptr<Impl> m_pImpl;

	public:
		explicit AccelerationStructure(Device& gpu, const std::vector<ASGeometryInstance>& geometries, const ASProceduralInjectorList& proceduralInjectors = {});
		explicit AccelerationStructure(Device& gpu, const CommandEncoder& encoder, const std::vector<ASGeometryInstance>& geometries, const ASProceduralInjectorList& proceduralInjectors = {});
		~AccelerationStructure();

		Impl& GetImpl();
		uintptr_t GetGpuAddress() const;

		static ASGeometry* CreateGeometry(Device& gpu, const ASGeometryInfo& info);
		static ASGeometry* CreateGeometryInline(Device& gpu, const CommandEncoder& encoder, const ASGeometryInfo& info);
		static ASGeometry* CreateAabb(Device& gpu, const std::array<glm::vec3, 2>& aabbMinMax);
		static ASGeometry* CreateAabbInline(Device& gpu, const CommandEncoder& encoder, const std::array<glm::vec3, 2>& aabbMinMax);
		static void        DestroyGeometry(ASGeometry*& geometry);
	};

	class AccelerationStructureBuilder
	{
		Device&                         m_gpu;
		std::vector<ASGeometryInstance> m_instances;

	public:
		explicit AccelerationStructureBuilder(Device& gpu);

		AccelerationStructureBuilder& AddInstance(const ASGeometryInstance& instance);
		AccelerationStructureBuilder& AddInstances(const std::vector<ASGeometryInstance>& geometries);
		std::unique_ptr<AccelerationStructure> Build();
		std::unique_ptr<AccelerationStructure> BuildInline(const CommandEncoder& encoder);
	};
}

#endif /* GALLIUM__GPU__ACCELERATIONSTRUCTURE_H */
