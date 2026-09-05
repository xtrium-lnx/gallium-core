#include <gallium/render/materiallibrary.h>

#include <gallium/gpu/buffer.h>
#include <gallium/gpu/image.h>
#include <gallium/gpu/descriptorregistry.h>

using namespace ga::render;

MaterialLibrary::MaterialLibrary()
{
	Clear();
}

MaterialLibrary::~MaterialLibrary()
{
}

void MaterialLibrary::Clear()
{
	m_materials.clear();
	m_materialIndices.clear();

	Material defaultMaterial;
	defaultMaterial.isImplicit          = true;
	defaultMaterial.baseColor.value     = glm::vec3(1.0f);
	defaultMaterial.normal.value        = glm::vec3(0.0f, 0.0f, 1.0f);
	defaultMaterial.orm.value           = glm::vec3(0.0f, 0.15f, 0.0f);
	defaultMaterial.emissive.value      = glm::vec3(0.0f);
	defaultMaterial.transmittance.value = glm::vec3(0.0f, 0.0f, 0.0f);
	defaultMaterial.alpha.value         = 1.0f;
	defaultMaterial.ior.value           = 1.5f;

	Add("default", defaultMaterial);
}

std::unordered_map<std::string, Material>& MaterialLibrary::Materials()
{
	return m_materials;
}

bool MaterialLibrary::Contains(const std::string& name)
{
	return m_materials.contains(name);
}

uint32_t MaterialLibrary::IndexOf(const std::string& name)
{
	return m_materialIndices[name];
}

std::string MaterialLibrary::NameOf(uint32_t i)
{
	for (auto& [name, index] : m_materialIndices)
		if (index == i)
			return name;

	return "(default)";
}

Material& MaterialLibrary::ValueOf(const std::string& name)
{
	return m_materials[name];
}

Material& MaterialLibrary::ValueOf(uint32_t i)
{
	return ValueOf(NameOf(i));
}

uint32_t MaterialLibrary::Add(const std::string& name, const Material& material)
{
	if (Contains(name))
		return uint32_t(-1);

	uint32_t index = uint32_t(m_materialIndices.size());

	m_materials[name]       = material;
	m_materialIndices[name] = index;
	m_isDirty = true;

	return index;
}

void MaterialLibrary::Remove(const std::string& name)
{
	if (!Contains(name))
		return;

	m_materialIndices.erase(name);
	m_materials.erase(name);
}

struct MaterialValueInternal
{
	glm::vec4  value;
	glm::uvec4 textureId;

	MaterialValueInternal() : value(0.0f), textureId(-1) {}
	MaterialValueInternal(const MaterialValue<float>& v)     : value(v.value, 0.0f, 0.0f, 0.0f), textureId(v.textureId, 0, 0, 0) {}
	MaterialValueInternal(const MaterialValue<glm::vec2>& v) : value(v.value, 0.0f, 0.0f), textureId(v.textureId, 0, 0, 0) {}
	MaterialValueInternal(const MaterialValue<glm::vec3>& v) : value(v.value, 0.0f), textureId(v.textureId, 0, 0, 0) {}
	MaterialValueInternal(const MaterialValue<glm::vec4>& v) : value(v.value), textureId(v.textureId, 0, 0, 0) {}
};

struct MaterialInternal
{
	MaterialValueInternal baseColor;
	MaterialValueInternal orm;
	MaterialValueInternal normal;
	MaterialValueInternal emissive;
	MaterialValueInternal transmittance;
	MaterialValueInternal alpha;
	MaterialValueInternal ior;

	MaterialInternal() = default;

	MaterialInternal(const Material& mat)
		: baseColor(mat.baseColor)
		, orm(mat.orm)
		, normal(mat.normal)
		, emissive(mat.emissive)
		, transmittance(mat.transmittance)
		, alpha(mat.alpha)
		, ior(mat.ior)
	{}
};

void MaterialLibrary::MarkDirty()
{
	m_isDirty = true;
}

void MaterialLibrary::Build(ga::gpu::Device& gpu)
{
	uint32_t maxIndex = 0;
	for (const auto& [k, v] : m_materialIndices)
		maxIndex = glm::max(maxIndex, v);

	std::vector<MaterialInternal> indexedMaterials(maxIndex + 1);

	for (const auto& [name, mat] : m_materials)
	{
		uint32_t index = m_materialIndices[name];
		indexedMaterials[index] = mat;
	}

	m_materialsBuffer = std::make_unique<ga::gpu::Buffer>(gpu, ga::gpu::EBufferUsage::UniformBuffer, indexedMaterials);
	m_isDirty = false;
}

void MaterialLibrary::Update(ga::gpu::Device& gpu)
{
	if (m_isDirty)
		Build(gpu);
}

ga::gpu::Buffer& MaterialLibrary::GetBuffer()
{
	return *m_materialsBuffer;
}
