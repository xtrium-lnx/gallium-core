#include "mesh_glb.h"

#include <gallium/assets/assetmanager.h>
#include <gallium/gpu/accelerationstructure.h>
#include <gallium/gpu/buffer.h>
#include <gallium/gpu/image.h>
#include <gallium/gpu/descriptorregistry.h>
#include <gallium/render/materiallibrary.h>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/norm.hpp>
#include <glm/gtx/matrix_decompose.hpp>

#include <fastgltf/core.hpp>
#include <fastgltf/types.hpp>
#include <fastgltf/tools.hpp>

#include <variant>

#ifndef NDEBUG
# pragma comment(lib, "fastgltf-d.lib")
#else
# pragma comment(lib, "fastgltf.lib")
#endif

using namespace ga::assets::loaders;

// ----------------------------------------------------------------------------

static std::vector<glm::vec3> s_ComputeTangents(
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

        const glm::vec3& p0  = positions[i0];
        const glm::vec3& p1  = positions[i1];
        const glm::vec3& p2  = positions[i2];
        const glm::vec2& uv0 = texcoords[i0];
        const glm::vec2& uv1 = texcoords[i1];
        const glm::vec2& uv2 = texcoords[i2];

        glm::vec3 edge1    = p1 - p0;
        glm::vec3 edge2    = p2 - p0;
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
    }

    for (size_t i = 0; i < tangents.size(); ++i)
    {
        const glm::vec3& n = normals[i];
        glm::vec3&       t = tangents[i];

        t = glm::normalize(t - n * glm::dot(n, t));

        if (glm::length2(t) < 0.001f)
        {
            if (glm::abs(n.x) < 0.9f)
                t = glm::normalize(glm::cross(n, glm::vec3(1.0f, 0.0f, 0.0f)));
            else
                t = glm::normalize(glm::cross(n, glm::vec3(0.0f, 1.0f, 0.0f)));
        }
    }

    return tangents;
}

static std::vector<glm::vec3> s_ComputeNormals(const std::vector<glm::vec3>& positions, const std::vector<uint32_t>&  indices)
{
    std::vector<glm::vec3> normals(positions.size(), glm::vec3(0.0f));

    for (size_t i = 0; i < indices.size(); i += 3)
    {
        uint32_t i0 = indices[i];
        uint32_t i1 = indices[i + 1];
        uint32_t i2 = indices[i + 2];

        const glm::vec3& p0 = positions[i0];
        const glm::vec3& p1 = positions[i1];
        const glm::vec3& p2 = positions[i2];

        glm::vec3 e1 = p1 - p0;
        glm::vec3 e2 = p2 - p0;

        glm::vec3 faceNormal = glm::cross(e1, e2);

        if (glm::length2(faceNormal) > 0.0f)
        {
            normals[i0] += faceNormal;
            normals[i1] += faceNormal;
            normals[i2] += faceNormal;
        }
    }

    for (auto& n : normals)
    {
        if (glm::length2(n) > 0.0f)
            n = glm::normalize(n);
        else
            n = glm::vec3(0.0f, 1.0f, 0.0f);
    }

    return normals;
}

// ----------------------------------------------------------------------------

static glm::mat4 s_GetLocalTransform(const fastgltf::Node& node)
{
    if (std::holds_alternative<fastgltf::math::fmat4x4>(node.transform))
        return glm::make_mat4(std::get<fastgltf::math::fmat4x4>(node.transform).data());
    

    auto& trs = std::get<fastgltf::TRS>(node.transform);

    glm::vec3 T = glm::make_vec3(trs.translation.data());
    glm::quat R = glm::make_quat(trs.rotation.data());
    glm::vec3 S = glm::make_vec3(trs.scale.data());

    return glm::translate(glm::mat4(1.0f), T)
         * glm::mat4_cast(R)
         * glm::scale(glm::mat4(1.0f), S);
}

static void s_TraverseNode(const fastgltf::Asset& asset, size_t nodeIndex, const glm::mat4& parentTransform, std::vector<glm::mat4>& outGlobalTransforms)
{
    const auto& node = asset.nodes[nodeIndex];

    glm::mat4 localTransform  = s_GetLocalTransform(node);
    glm::mat4 globalTransform = parentTransform * localTransform;
    outGlobalTransforms[nodeIndex] = globalTransform;

    for (auto child : node.children)
        s_TraverseNode(asset, child, globalTransform, outGlobalTransforms);
}

static std::vector<glm::mat4> s_ComputeSceneTransforms(const fastgltf::Asset& asset)
{
    std::vector<glm::mat4> result(asset.nodes.size());
    
    for (auto root : asset.scenes[asset.defaultScene.value_or(0)].nodeIndices)
        s_TraverseNode(asset, root, glm::mat4(1.0f), result);

    return result;
}

// ----------------------------------------------------------------------------

ga::render::AnimationClip s_LoadClip(const fastgltf::Asset& asset, const fastgltf::Animation& anim, const std::unordered_map<size_t, uint32_t>& nodeToJoint)
{
    ga::render::AnimationClip clip;
    clip.name = anim.name;

    for (auto& channel : anim.channels)
    {
        if (!channel.nodeIndex.has_value())
            continue;

        auto it = nodeToJoint.find(*channel.nodeIndex);
        if (it == nodeToJoint.end())
            continue; // targets a non-skeleton node (ie. camera)

        using Path = ga::render::AnimationChannel::Path;
        using Interp = ga::render::AnimationSampler::Interpolation;

        std::optional<Path> path;
        switch (channel.path)
        {
        case fastgltf::AnimationPath::Translation: path = Path::Translation; break;
        case fastgltf::AnimationPath::Rotation:    path = Path::Rotation;    break;
        case fastgltf::AnimationPath::Scale:       path = Path::Scale;       break;
        default: break; // Weights (morph targets) - not supported
        }
        if (!path.has_value())
            continue;

        auto& s = anim.samplers[channel.samplerIndex];

        ga::render::AnimationSampler sampler;
        sampler.interpolation = [&] {
            switch (s.interpolation) {
            case fastgltf::AnimationInterpolation::Step:        return Interp::Step;
            case fastgltf::AnimationInterpolation::CubicSpline: return Interp::CubicSpline;
            default:                                            return Interp::Linear;
            }
        }();

        // Input accessor: timestamps
        {
            auto& acc = asset.accessors[s.inputAccessor];
            sampler.times.resize(acc.count);
            fastgltf::copyFromAccessor<float>(asset, acc, sampler.times.data());
            clip.duration = std::max(clip.duration, sampler.times.back());
        }

        // Output accessor: vec3 for T/S, vec4 for R
        // CubicSpline layout: [inTangent, value, outTangent] per keyframe -> 3x count
        // We store raw and let Update() index correctly per interpolation mode
        {
            auto& acc = asset.accessors[s.outputAccessor];
            sampler.values.resize(acc.count);

            if (acc.type == fastgltf::AccessorType::Vec3)
            {
                fastgltf::iterateAccessor<fastgltf::math::fvec3>(asset, acc, [&, i = size_t(0)](const fastgltf::math::fvec3& v) mutable {
                    sampler.values[i++] = { v.x(), v.y(), v.z(), 1.0f };
                });
            }
            else fastgltf::iterateAccessor<fastgltf::math::fvec4>(asset, acc, [&, i = size_t(0)](const fastgltf::math::fvec4& v) mutable {
                sampler.values[i++] = { v.x(), v.y(), v.z(), v.w() };
            });
        }

        clip.channels.push_back({
            .jointIndex = it->second,
            .path = *path,
            .sampler = std::move(sampler)
        });
    }

    return clip;
}

ga::render::Skin s_LoadSkin(const fastgltf::Asset& asset, const fastgltf::Skin& skin, const std::unordered_map<size_t, uint32_t>& nodeToJoint)
{
    ga::render::Skin result;
    result.skeleton.resize(skin.joints.size());

    // Inverse bind matrices - identity if absent (see spec - Skins)
    std::vector<glm::mat4> ibms(skin.joints.size(), glm::mat4(1.f));
    if (skin.inverseBindMatrices.has_value())
    {
        auto& acc = asset.accessors[*skin.inverseBindMatrices];
        fastgltf::copyFromAccessor<fastgltf::math::fmat4x4>(asset, acc,
            reinterpret_cast<fastgltf::math::fmat4x4*>(ibms.data()));
    }

    for (uint32_t j = 0; j < uint32_t(skin.joints.size()); ++j)
    {
        const auto& node = asset.nodes[skin.joints[j]];
        ga::render::Joint& joint = result.skeleton[j];

        joint.name = node.name;
        joint.inverseBindMatrix = ibms[j];
        joint.parentIndex = -1;

        // Local TRS - prefer TRS variant, decompose matrix only as fallback
        std::visit(fastgltf::visitor {
            [&](const fastgltf::TRS& trs) {
                joint.localTranslation = glm::make_vec3(trs.translation.data());
                joint.localRotation    = glm::make_quat(trs.rotation.data());
                joint.localScale       = glm::make_vec3(trs.scale.data());
            },
            [&](const fastgltf::math::fmat4x4& mat) {
                glm::vec3 skew; glm::vec4 persp;
                glm::decompose(glm::make_mat4(mat.data()), joint.localScale, joint.localRotation, joint.localTranslation, skew, persp);
            }
        }, node.transform);

        // Resolve parent: find the joint whose node lists joint j's node as a child
        for (uint32_t k = 0; k < uint32_t(skin.joints.size()); ++k)
        {
            for (auto childNodeIndex : asset.nodes[skin.joints[k]].children)
            {
                auto it = nodeToJoint.find(childNodeIndex);
                if (it != nodeToJoint.end() && it->second == j)
                    joint.parentIndex = int(k);
            }
        }
    }

    return result;
}

// ----------------------------------------------------------------------------

uint32_t s_LoadTexture(std::string_view meshPath, ga::render::MaterialLibrary& matlib, const fastgltf::Asset& asset, ga::assets::AssetManager& assetManager, ga::gpu::Device& gpu, const fastgltf::TextureInfo& info)
{
    const auto& tex = asset.textures[info.textureIndex];
    if (!tex.imageIndex.has_value())
        return uint32_t(-1);

    const auto& image = asset.images[*tex.imageIndex];
    // TODO: Check if this image already exists, return descriptor slot if it does
    
    std::string imageName(image.name);

    if (imageName.empty())
    {
        static size_t unnamedIndex = 0;
        imageName = std::format("unnamed_{}", unnamedIndex++);
    }

    return std::visit(fastgltf::visitor {
        [](const auto& a) { return uint32_t(-1); },
        [&](const fastgltf::sources::URI& u) { /* unsupported for now because of VFS indirection */return uint32_t(-1); },
        [&](const fastgltf::sources::BufferView& view) {
            const auto& bufferView = asset.bufferViews[view.bufferViewIndex];
            const auto& buffer     = asset.buffers[bufferView.bufferIndex];

            switch (view.mimeType)
            {
            case fastgltf::MimeType::JPEG:
                imageName += ".jpg";
                break;
            case fastgltf::MimeType::PNG:
                imageName += ".png";
                break;
            default:
                break;
            }

            return std::visit(fastgltf::visitor {
                [](const auto& arg) { return uint32_t(-1); },
                [&](const fastgltf::sources::Array& vector) {
                    auto dataSpan = std::span(vector.bytes.data() + bufferView.byteOffset, bufferView.byteLength);
                    auto localImageName = std::format("{}/lattice_recurse/{}", meshPath, imageName);

                    auto& image = assetManager.IsLoaded(imageName)
                        ? assetManager.Get<ga::gpu::Image>(localImageName)->get()
                        : assetManager.Insert<ga::gpu::Image>(localImageName, dataSpan);

                    return image.GetDescriptorIndex().slot;
                }
            }, buffer.data);
        }
    }, image.data);
}

uint32_t s_LoadMaterial(std::string_view meshPath, ga::render::MaterialLibrary& matlib, const fastgltf::Asset& asset, ga::assets::AssetManager& assetManager, ga::gpu::Device& gpu, const fastgltf::Material& material)
{
    std::string matName(material.name);
    if (matlib.Contains(matName))
        return matlib.IndexOf(matName);

    ga::render::Material result;
    result.isImplicit = true;

    result.baseColor = {
        .value     = { material.pbrData.baseColorFactor.x(), material.pbrData.baseColorFactor.y(), material.pbrData.baseColorFactor.z() },
        .textureId = material.pbrData.baseColorTexture.has_value()
            ? s_LoadTexture(meshPath, matlib, asset, assetManager, gpu, *material.pbrData.baseColorTexture)
            : uint32_t(-1)
    };

    result.orm = {
        .value = { 0.0f, material.pbrData.roughnessFactor, material.pbrData.metallicFactor },
        .textureId = material.pbrData.metallicRoughnessTexture.has_value()
            ? s_LoadTexture(meshPath, matlib, asset, assetManager, gpu, *material.pbrData.metallicRoughnessTexture)
            : uint32_t(-1)
    };

    result.normal = {
        .value = { 0.0f, 0.0f, 1.0f },
        .textureId = material.normalTexture.has_value()
            ? s_LoadTexture(meshPath, matlib, asset, assetManager, gpu, *material.normalTexture)
            : uint32_t(-1)
    };

    result.emissive = {
        .value = material.emissiveStrength * glm::vec3 { material.emissiveFactor.x(), material.emissiveFactor.y(), material.emissiveFactor.z() },
        .textureId = material.emissiveTexture.has_value()
            ? s_LoadTexture(meshPath, matlib, asset, assetManager, gpu, *material.emissiveTexture)
            : uint32_t(-1)
    };

    result.alpha = {
        .value = material.pbrData.baseColorFactor.w(),
        .textureId = uint32_t(-1)
    };

    result.transmittance = {
        .value = { material.pbrData.baseColorFactor.x(), material.pbrData.baseColorFactor.y(), material.pbrData.baseColorFactor.z() },
        .textureId = uint32_t(-1)
    };

    result.ior = {
        .value     = material.ior,
        .textureId = uint32_t(-1)
    };

    return matlib.Add(matName, result);
}

// ----------------------------------------------------------------------------

GlbMeshLoader::GlbMeshLoader(ga::gpu::Device& gpu)
    : m_gpu(gpu)
{
}

bool GlbMeshLoader::CanLoad(std::string_view ext) const
{
    return ext == ".glb";
}

std::shared_ptr<ga::render::Mesh> GlbMeshLoader::Load(std::string_view path, std::span<const std::byte> bytes, AssetManager& assetManager)
{
    using ga::render::EVertexStream;
    fastgltf::Parser parser;

    auto data = fastgltf::GltfDataBuffer::FromBytes(bytes.data(), bytes.size());
    if (data.error() != fastgltf::Error::None)
        throw std::runtime_error(std::format("GLB data error: {}", fastgltf::getErrorMessage(data.error())));

    auto asset = parser.loadGltfBinary(data.get(), {}, fastgltf::Options::None);
    auto assetTransforms = s_ComputeSceneTransforms(asset.get());

    if (asset.error() != fastgltf::Error::None)
        throw std::runtime_error(std::format("GLB parse error: {}", fastgltf::getErrorMessage(asset.error())));

    if (asset->meshes.empty())
        throw std::runtime_error("GLB contains no meshes");

    std::unordered_map<size_t, uint32_t> nodeToJoint;
    if (!asset->skins.empty())
    {
        auto& skin = asset->skins[0];
        for (uint32_t j = 0; j < uint32_t(skin.joints.size()); ++j)
            nodeToJoint[skin.joints[j]] = j;
    }

    auto result = std::make_shared<ga::render::Mesh>();

    std::vector<uint32_t>   indices;
    std::vector<glm::vec3>  positions;
    std::vector<glm::vec3>  normals;
    std::vector<glm::vec2>  uvs;
    std::vector<glm::vec3>  tangents;
    std::vector<glm::uvec4> joints;
    std::vector<glm::vec4>  weights;

    bool hasTangents = false;
    bool hasSkinData = false;

    for (size_t nodeIndex = 0; nodeIndex < asset->nodes.size(); ++nodeIndex)
    {
        const auto& node = asset->nodes[nodeIndex];
        if (!node.meshIndex.has_value())
            continue;

        const glm::mat4& M    = assetTransforms[nodeIndex];
        glm::mat4        N    = glm::transpose(glm::inverse(M));
        float            det  = glm::determinant(glm::mat3(M));
        const auto&      mesh = asset->meshes[*node.meshIndex];

        for (auto& prim : mesh.primitives)
        {
            uint32_t material = prim.materialIndex.has_value()
                ? s_LoadMaterial(path, assetManager.MaterialLibrary(), asset.get(), assetManager, m_gpu, asset->materials[*prim.materialIndex])
                : assetManager.MaterialLibrary().IndexOf("default");

            if (prim.type != fastgltf::PrimitiveType::Triangles)
                continue;

            auto posIt = prim.findAttribute("POSITION");
            if (posIt == prim.attributes.end())
                continue;

            bool hasIndices = prim.indicesAccessor.has_value();

            const uint32_t vertexBase = uint32_t(positions.size());
            const size_t   firstIndex = indices.size();

            {
                auto& acc = asset->accessors[posIt->accessorIndex];
                positions.resize(vertexBase + acc.count);
                fastgltf::copyFromAccessor<fastgltf::math::fvec3>(asset.get(), acc, reinterpret_cast<fastgltf::math::fvec3*>(positions.data() + vertexBase));
            }

            {
                if (hasIndices)
                {
                    auto& acc = asset->accessors[*prim.indicesAccessor];
                    indices.reserve(indices.size() + acc.count);
                    fastgltf::iterateAccessor<uint32_t>(asset.get(), acc, [&](uint32_t i) { indices.push_back(i + vertexBase); });
                }
                else
                {
                    auto& acc = asset->accessors[posIt->accessorIndex];
                    indices.reserve(indices.size() + acc.count);

                    for (uint32_t i = 0; i < acc.count; ++i)
                        indices.push_back(vertexBase + i);
                }

                if (det < 0.0f)
                    for (size_t i = firstIndex; i < indices.size(); i += 3)
                        std::swap(indices[i + 1], indices[i + 2]);
            }

            if (auto it = prim.findAttribute("NORMAL"); it != prim.attributes.end())
            {
                auto& acc = asset->accessors[it->accessorIndex];
                normals.resize(vertexBase + acc.count, glm::vec3(0.f));
                fastgltf::copyFromAccessor<fastgltf::math::fvec3>(asset.get(), acc, reinterpret_cast<fastgltf::math::fvec3*>(normals.data() + vertexBase));
            }

            
            if (auto it = prim.findAttribute("TEXCOORD_0"); it != prim.attributes.end())
            {
                auto& acc = asset->accessors[it->accessorIndex];
                uvs.resize(vertexBase + acc.count, glm::vec2(0.f));
                fastgltf::copyFromAccessor<fastgltf::math::fvec2>(asset.get(), acc, reinterpret_cast<fastgltf::math::fvec2*>(uvs.data() + vertexBase));
            }

            
            if (auto it = prim.findAttribute("TANGENT"); it != prim.attributes.end())
            {
                auto& acc = asset->accessors[it->accessorIndex];
                tangents.resize(vertexBase + acc.count, glm::vec3(0.f));

                std::vector<fastgltf::math::fvec4> tmp(acc.count);
                fastgltf::copyFromAccessor<fastgltf::math::fvec4>(asset.get(), acc, tmp.data());
                for (size_t i = 0; i < acc.count; ++i)
                    tangents[vertexBase + i] = glm::vec3(tmp[i].x(), tmp[i].y(), tmp[i].z());

                hasTangents = true;
            }

            if (auto it = prim.findAttribute("JOINTS_0"); it != prim.attributes.end())
            {
                auto& acc = asset->accessors[it->accessorIndex];
                joints.resize(vertexBase + acc.count, glm::uvec4(0));
                fastgltf::copyFromAccessor<fastgltf::math::uvec4>(asset.get(), acc, reinterpret_cast<fastgltf::math::uvec4*>(joints.data() + vertexBase));
                hasSkinData = true;
            }

            if (auto it = prim.findAttribute("WEIGHTS_0"); it != prim.attributes.end())
            {
                auto& acc = asset->accessors[it->accessorIndex];
                weights.resize(vertexBase + acc.count, glm::vec4(0.f));
                fastgltf::copyFromAccessor<fastgltf::math::fvec4>(asset.get(), acc, reinterpret_cast<fastgltf::math::fvec4*>(weights.data() + vertexBase));

                for (size_t i = vertexBase; i < weights.size(); ++i)
                {
                    float sum = weights[i].x + weights[i].y + weights[i].z + weights[i].w;
                    if (sum > 0.f)
                        weights[i] /= sum;
                }
            }

            std::vector<glm::mat4> localMatrices = { M, N };
            result->primitives.push_back(ga::render::Mesh::Primitive {
                .name          = std::string(mesh.name),
                .materialIndex = material,
                .startIndex    = uint32_t(firstIndex),
                .indexCount    = uint32_t(indices.size() - firstIndex),
                .localTransformBuffer = std::make_unique<ga::gpu::Buffer>(m_gpu, ga::gpu::BufferInfo {
                    .usage       = ga::gpu::EBufferUsage::UniformBuffer,
                    .size        = 2 * sizeof(glm::mat4),
                    .initialData = localMatrices.data()
                }),
                .localTransform = M
            });

            result->primitives.back().localTransformBuffer->SetDebugName(std::format("GLB ({}): primitive local transform", path));
        }
    }

    if (positions.empty())
        throw std::runtime_error("GLB contains no usable geometry");

    if (normals.empty())
        normals = s_ComputeNormals(positions, indices);

    if (!hasTangents && !normals.empty() && !uvs.empty())
        tangents = s_ComputeTangents(positions, normals, uvs, indices);

    result->UploadStream<EVertexStream::Position>(m_gpu, positions);
    result->UploadStream<EVertexStream::Normal>(m_gpu, normals);

    if (!uvs.empty())
        result->UploadStream<EVertexStream::UV>(m_gpu, uvs);

    if (!tangents.empty())
        result->UploadStream<EVertexStream::Tangent>(m_gpu, tangents);

    if (hasSkinData && !joints.empty() && !weights.empty())
    {
        result->UploadStream<EVertexStream::Joints>(m_gpu, joints);
        result->UploadStream<EVertexStream::Weights>(m_gpu, weights);
    }

    result->UploadIndices(m_gpu, indices);

    if (!asset->skins.empty() && hasSkinData)
    {
        auto skin = s_LoadSkin(asset.get(), asset->skins[0], nodeToJoint);

        skin.jointMatrixBuffer = std::make_unique<ga::gpu::Buffer>(m_gpu, ga::gpu::BufferInfo{
            .usage                   = ga::gpu::EBufferUsage::StorageBuffer,
            .size                    = skin.skeleton.size() * sizeof(glm::mat4),
            .createPersistentStaging = true
        });

        skin.jointMatrixBuffer->SetDebugName(std::format("GLB ({}): joint matrices", path));

        for (auto& anim : asset->animations)
            skin.clips.push_back(s_LoadClip(asset.get(), anim, nodeToJoint));

        result->skin = std::move(skin);
    }

    result->indexCount  = uint32_t(indices.size());
    result->vertexCount = uint32_t(positions.size());
    result->UpdateAccelerationStructures(m_gpu);

    return result;
}
