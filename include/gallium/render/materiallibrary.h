#ifndef GALLIUM__RENDER__MATERIALLIBRARY_H
#define GALLIUM__RENDER__MATERIALLIBRARY_H
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>

#include <glm/glm.hpp>

namespace ga::gpu
{
	class Device;
	class Buffer;
	class Image;
}

namespace ga::render
{
	template<typename T>
	struct MaterialValue
	{
		T           value;
		std::string texturePath;
		uint32_t    textureId = uint32_t(-1);
	};

	struct Material
	{
		MaterialValue<glm::vec3> baseColor;
		MaterialValue<glm::vec3> orm;
		MaterialValue<glm::vec3> normal;
		MaterialValue<glm::vec3> emissive;
		MaterialValue<glm::vec3> transmittance;
		MaterialValue<float>     alpha;
		MaterialValue<float>     ior;
		bool                     isImplicit = false;
	};

	class MaterialLibrary
	{
		std::unordered_map<std::string, Material> m_materials;
		std::unordered_map<std::string, uint32_t> m_materialIndices;
		std::unique_ptr<ga::gpu::Buffer>          m_materialsBuffer;

		bool m_isDirty = false;

	public:
		MaterialLibrary();
		~MaterialLibrary();

		void Clear();

		std::unordered_map<std::string, Material>& Materials();
		bool        Contains(const std::string& name);
		uint32_t    IndexOf(const std::string& name);
		std::string NameOf(uint32_t i);
		Material&   ValueOf(const std::string& name);
		Material&   ValueOf(uint32_t i);

		uint32_t Add(const std::string& name, const Material& material);
		void     Remove(const std::string& name);

		void MarkDirty();
		void Build(ga::gpu::Device& gpu);
		void Update(ga::gpu::Device& gpu);
		ga::gpu::Buffer& GetBuffer();
	};
}

#endif /* GALLIUM__RENDER__MATERIALLIBRARY_H */
