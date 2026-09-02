#include "mesh_obj.h"

#include <gallium/gpu/accelerationstructure.h>
#include <gallium/gpu/buffer.h>
#include <gallium/assets/assetmanager.h>
#include <gallium/render/materiallibrary.h>

#include <streambuf>
#include <istream>

#include <glm/glm.hpp>
#include <glm/gtx/norm.hpp>

#include <rapidobj.hpp>

using namespace ga::assets::loaders;

// ----------------------------------------------------------------------------

struct InMemoryBuffer
    : std::streambuf
{
    InMemoryBuffer(const char* begin, const char* end)
        : begin(const_cast<char*>(begin))
        , end(const_cast<char*>(end))
    {
        setg(this->begin, this->begin, this->end);
    }

    InMemoryBuffer(std::span<const std::byte> bytes)
        : InMemoryBuffer(
            reinterpret_cast<const char*>(bytes.data()),
            reinterpret_cast<const char*>(bytes.data()) + bytes.size_bytes()
        )
    {}

    virtual pos_type seekoff(off_type off, std::ios_base::seekdir dir, std::ios_base::openmode which = std::ios_base::in) override
    {
        if (dir == std::ios_base::cur)
            gbump(int(off));
        else if (dir == std::ios_base::end)
            setg(begin, end + off, end);
        else if (dir == std::ios_base::beg)
            setg(begin, begin + off, end);

        return gptr() - eback();
    }

    virtual pos_type seekpos(std::streampos pos, std::ios_base::openmode mode) override
    {
        return seekoff(pos - pos_type(off_type(0)), std::ios_base::beg, mode);
    }

    char* begin;
    char* end;
};

struct IndexKey
{
    int position_index;
    int normal_index;
    int texcoord_index;

    bool operator==(const IndexKey& other) const
    {
        return position_index == other.position_index
            && normal_index   == other.normal_index
            && texcoord_index == other.texcoord_index;
    }
};

struct IndexKeyHash
{
    size_t operator()(const IndexKey& key) const
    {
        return (size_t)key.position_index ^ ((size_t)key.normal_index << 10) ^ ((size_t)key.texcoord_index << 20);
    }
};

// ----------------------------------------------------------------------------

std::vector<glm::vec3> s_ComputeTangents(
    const std::vector<glm::vec3>& positions,
    const std::vector<glm::vec3>& normals,
    const std::vector<glm::vec2>& texcoords,
    const std::vector<uint32_t>&  indices
) {
    std::vector<glm::vec3> tangents(normals.size(), glm::vec3(0.0f));

    for (size_t i = 0; i < indices.size(); i += 3)
    {
        uint32_t i0 = indices[i];
        uint32_t i1 = indices[i + 1];
        uint32_t i2 = indices[i + 2];

        const glm::vec3& p0 = positions[i0];
        const glm::vec3& p1 = positions[i1];
        const glm::vec3& p2 = positions[i2];
        const glm::vec2& uv0 = texcoords[i0];
        const glm::vec2& uv1 = texcoords[i1];
        const glm::vec2& uv2 = texcoords[i2];

        glm::vec3 edge1 = p1 - p0;
        glm::vec3 edge2 = p2 - p0;
        glm::vec2 deltaUV1 = uv1 - uv0;
        glm::vec2 deltaUV2 = uv2 - uv0;

        float f = 1.0f / (deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y);

        if (std::isfinite(f))
        {
            glm::vec3 tangent;
            tangent.x = f * (deltaUV2.y * edge1.x - deltaUV1.y * edge2.x);
            tangent.y = f * (deltaUV2.y * edge1.y - deltaUV1.y * edge2.y);
            tangent.z = f * (deltaUV2.y * edge1.z - deltaUV1.y * edge2.z);

            tangents[i0] += tangent;
            tangents[i1] += tangent;
            tangents[i2] += tangent;
        }

        for (size_t i = 0; i < tangents.size(); ++i)
        {
            const glm::vec3& n = normals[i];
            glm::vec3& t = tangents[i];

            // Gram-Schmidt orthogonalization
            t = glm::normalize(t - n * glm::dot(n, t));

            if (glm::length2(t) < 0.001f)
            {
                if (glm::abs(n.x) < 0.9f)
                    t = glm::normalize(glm::cross(n, glm::vec3(1.0f, 0.0f, 0.0f)));
                else
                    t = glm::normalize(glm::cross(n, glm::vec3(0.0f, 1.0f, 0.0f)));
            }
        }
    }

    return tangents;
}

// ----------------------------------------------------------------------------

ObjMeshLoader::ObjMeshLoader(ga::gpu::Device& gpu)
    : m_gpu(gpu)
{
}

bool ObjMeshLoader::CanLoad(std::string_view ext) const
{
    return ext == ".obj";
}

std::shared_ptr<ga::render::Mesh>  ObjMeshLoader::Load(std::string_view path, std::span<const std::byte> bytes, AssetManager& assetManager)
{
    using ga::render::EVertexStream;

    auto result = std::make_shared<ga::render::Mesh>();

    InMemoryBuffer buffer(bytes);
    std::istream is(&buffer);

    auto objData = rapidobj::ParseStream(is);
    if (!rapidobj::Triangulate(objData))
        throw std::runtime_error("Unable to triangulate mesh data");

    std::vector<uint32_t>                                indices;
    std::unordered_map<IndexKey, uint32_t, IndexKeyHash> indexHashmap;

    for (const auto& shape : objData.shapes)
    {
        if (shape.mesh.indices.empty())
            continue;

        size_t firstIndex = indices.size();

        for (const auto& index : shape.mesh.indices)
        {
            IndexKey key{ index.position_index, index.normal_index, index.texcoord_index };
            if (!indexHashmap.contains(key))
                indexHashmap[key] = uint32_t(indexHashmap.size());

            indices.push_back(indexHashmap[key]);
        }

        std::vector<glm::mat4> localMatrices = { glm::mat4(1.0f), glm::mat4(1.0f) };
        result->primitives.push_back(render::Mesh::Primitive {
            .name          = shape.name,
            .materialIndex = assetManager.MaterialLibrary().IndexOf("default"),
            .startIndex    = uint32_t(firstIndex),
            .indexCount    = uint32_t(indices.size() - firstIndex),
            .localTransformBuffer = std::make_unique<ga::gpu::Buffer>(m_gpu, ga::gpu::BufferInfo {
                .usage       = ga::gpu::EBufferUsage::UniformBuffer,
                .size        = 2 * sizeof(glm::mat4),
                .initialData = localMatrices.data()
            }),
            .localTransform = glm::mat4(1.0f)
        });
        result->primitives.back().localTransformBuffer->SetDebugName(std::format("OBJ ({}): primitive local transform", path));
    }

    result->UploadIndices(m_gpu, indices);

    std::vector<glm::vec3> positions(indexHashmap.size());
    for (auto& [k, index] : indexHashmap)
        positions[index] = glm::vec3(
            objData.attributes.positions[k.position_index * 3 + 0],
            objData.attributes.positions[k.position_index * 3 + 1],
            objData.attributes.positions[k.position_index * 3 + 2]
        );

    result->UploadStream<EVertexStream::Position>(m_gpu, positions);

    std::vector<glm::vec3> normals;
    if (!objData.attributes.normals.empty())
    {
        normals.resize(indexHashmap.size(), glm::vec3(0.0f));
        for (auto& [k, index] : indexHashmap)
            normals[index] = glm::vec3(
                objData.attributes.normals[k.normal_index * 3 + 0],
                objData.attributes.normals[k.normal_index * 3 + 1],
                objData.attributes.normals[k.normal_index * 3 + 2]
            );

        result->UploadStream<EVertexStream::Normal>(m_gpu, normals);
    }

    std::vector<glm::vec2> uvs;
    if (!objData.attributes.texcoords.empty())
    {
        uvs.resize(indexHashmap.size(), glm::vec2(0.0f));
        for (auto& [k, index] : indexHashmap)
            uvs[index] = glm::vec2(
                objData.attributes.texcoords[k.texcoord_index * 2 + 0],
                objData.attributes.texcoords[k.texcoord_index * 2 + 1]
            );

        result->UploadStream<EVertexStream::UV>(m_gpu, uvs);
    }

    if (!normals.empty() && !uvs.empty())
    {
        std::vector<glm::vec3> tangents = s_ComputeTangents(positions, normals, uvs, indices);
        result->UploadStream<EVertexStream::Tangent>(m_gpu, tangents);
    }

    result->indexCount  = uint32_t(indices.size());
    result->vertexCount = uint32_t(positions.size());
    result->UpdateAccelerationStructures(m_gpu);

    return result;
}
