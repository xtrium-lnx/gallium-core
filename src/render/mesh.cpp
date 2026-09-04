#include <gallium/render/mesh.h>

#include <gallium/platform/vfs.h>
#include <gallium/gpu/accelerationstructure.h>
#include <gallium/gpu/computepipeline.h>
#include <gallium/gpu/shadermodule.h>
#include <gallium/gpu/shadermodule_reflection.h>

#include <glm/gtc/quaternion.hpp>

#include <ranges>

using namespace ga::render;

Mesh::~Mesh()
{
    for (auto& p : primitives)
        if (p.asGeometry)
            gpu::AccelerationStructure::DestroyGeometry(p.asGeometry);

	for (auto& vb : vertexBuffers)
		vb.reset();

	indexBuffer.reset();
}

void Mesh::UploadIndices(gpu::Device& gpu, std::span<uint32_t> data)
{
	indexBuffer = std::make_unique<ga::gpu::Buffer>(gpu, ga::gpu::BufferInfo {
        .usage       = ga::gpu::EBufferUsage::IndexBuffer | (gpu.SupportsRaytracing() ? gpu::EBufferUsage::AccelerationStructureBuildReadonly : gpu::EBufferUsage::None),
        .size        = data.size_bytes(),
        .initialData = data.data()
    });
    indexBuffer->SetDebugName("Mesh: index buffer");
}

void Mesh::UpdateAccelerationStructures(gpu::Device& gpu, const gpu::CommandEncoder* encoder /* = nullptr */)
{
    for (auto& p : primitives)
    {
        if (p.asGeometry)
            gpu.Defer([&gpu, blas = p.asGeometry]() { auto* as = blas; ga::gpu::AccelerationStructure::DestroyGeometry(as); });

        auto asInfo = ga::gpu::ASGeometryInfo {
            .vertexFormat = gpu::EFormat::R32G32B32_SFloat,
            .vertexBuffer = processedVertexBuffers[0] ? processedVertexBuffers[0].get() : vertexBuffers[0].get(),
            .indexBuffer  = indexBuffer.get(),
            .firstVertex  = 0,
            .firstIndex   = p.startIndex,
            .indexCount   = p.indexCount
        };

        if (encoder)
            p.asGeometry = gpu::AccelerationStructure::CreateGeometryInline(gpu, *encoder, asInfo);
        else
            p.asGeometry = gpu::AccelerationStructure::CreateGeometry(gpu, asInfo);
    }
}

void Mesh::Draw(gpu::RenderEncoder& encoder, uint32_t numInstances /* = 1 */) const
{
	std::vector<std::pair<gpu::Buffer*, size_t>> buffers;
	for (auto& b : vertexBuffers)
	{
		if (b)
			buffers.push_back(std::make_pair(b.get(), 0));
		else
			buffers.push_back(std::make_pair(nullptr, 0));
	}

	std::vector<std::tuple<uint32_t, uint32_t, uintptr_t, uintptr_t>> submeshes;
    for (auto& p : primitives)
    {
        if (p.enabled)
        {
            uintptr_t jointMatrixBuffer = skin
                ? skin->jointMatrixBuffer->GetGpuAddress()
                : 0Ui64;

            submeshes.push_back({ p.startIndex, p.indexCount, p.localTransformBuffer->GetGpuAddress(), jointMatrixBuffer });
        }
    }

	encoder.BindVertexBuffers(0, buffers);
	encoder.BindIndexBuffer(indexBuffer.get(), 0);

	for (const auto& [firstIndex, indexCount, localTransformBuffer, jointMatrixBuffer] : submeshes)
	{
		const std::vector<uintptr_t> submeshMatrices = { localTransformBuffer, jointMatrixBuffer };
		encoder.PushConstants(0, 2 * sizeof(uintptr_t), submeshMatrices.data());
		encoder.DrawIndexed(indexCount, numInstances, firstIndex, 0, 0);
	}
}

MeshInstance::MeshInstance(Mesh* mesh)
	: mesh(mesh)
{
	for (auto& p : mesh->primitives)
		primitiveEnablement.push_back(p.enabled);
}

// ----------------------------------------------------------------------------

uint32_t Skin::FindClip(const std::string& name)
{
    for (uint32_t i = 0; i < clips.size(); ++i)
    {
        const auto& c = clips[i];
        if (c.name == name)
            return i;
    }

    return uint32_t(-1);
}

std::unique_ptr<ga::gpu::ComputePipeline>      Skin::ms_skinPipeline = nullptr;
std::unique_ptr<ga::gpu::ShaderStructInstance> Skin::ms_skinPC       = nullptr;

void Skin::CreatePipeline(ga::platform::Vfs& vfs, ga::gpu::Device& gpu)
{
    auto shader = vfs.Open("/shaders/apply_skinning.slang.spv")
        .and_then([](auto f) { return f.ReadBytes(); })
        .transform([&gpu](auto bytes) { return std::make_unique<ga::gpu::ShaderModule>(gpu, bytes.data(), bytes.size()); })
        .value();

    ms_skinPipeline = std::make_unique<ga::gpu::ComputePipeline>(gpu, ga::gpu::ComputePipelineInfo{
        .stage = { *shader, ga::gpu::EShaderStage::Compute, "cs_main" }
    });

    ms_skinPC = std::make_unique<ga::gpu::ShaderStructInstance>(
        std::make_shared<ga::gpu::ShaderStructDesc>(*shader->GetStruct("PushConstants"))
    );
}

void Skin::DestroyPipeline()
{
    ms_skinPC       = nullptr;
    ms_skinPipeline = nullptr;
}

void Skin::Update(const ga::gpu::TransferEncoder& encoder, float time, uint32_t clipIndex)
{
    if (clipIndex != uint32_t(-1))
    {
        auto& clip = clips[clipIndex];
        time = glm::mod(time, clip.duration);

        for (auto& channel : clip.channels)
        {
            auto& joint   = skeleton[channel.jointIndex];
            auto& sampler = channel.sampler;

            auto it = std::upper_bound(sampler.times.begin(), sampler.times.end(), time);
            if (it == sampler.times.end()) { --it; }

            size_t i1 = std::distance(sampler.times.begin(), it);
            size_t i0 = i1 > 0 ? i1 - 1 : 0;
            float  t  = (sampler.times[i1] == sampler.times[i0])
                ? 0.0f
                : (time - sampler.times[i0]) / (sampler.times[i1] - sampler.times[i0]);

            // TODO implement CubicSpline interpolation support
            switch (channel.path)
            {
            case AnimationChannel::Path::Translation:
                joint.localTranslation = sampler.interpolation == AnimationSampler::Interpolation::Step
                    ? glm::vec3(sampler.values[i0])
                    : glm::mix(glm::vec3(sampler.values[i0]), glm::vec3(sampler.values[i1]), t);
                break;

            case AnimationChannel::Path::Rotation:
                joint.localRotation = sampler.interpolation == AnimationSampler::Interpolation::Step
                    ? glm::quat(sampler.values[i0].w, sampler.values[i0].x, sampler.values[i0].y, sampler.values[i0].z)
                    : glm::slerp(
                        glm::quat(sampler.values[i0].w, sampler.values[i0].x, sampler.values[i0].y, sampler.values[i0].z),
                        glm::quat(sampler.values[i1].w, sampler.values[i1].x, sampler.values[i1].y, sampler.values[i1].z),
                        t
                    );
                break;

            case AnimationChannel::Path::Scale:
                joint.localScale = sampler.interpolation == AnimationSampler::Interpolation::Step
                    ? glm::vec3(sampler.values[i0])
                    : glm::mix(glm::vec3(sampler.values[i0]), glm::vec3(sampler.values[i1]), t);
                break;
            }
        }

        for (auto& joint : skeleton)
            joint.globalTransform = joint.parentIndex >= 0
            ? skeleton[joint.parentIndex].globalTransform * joint.LocalMatrix()
            : joint.LocalMatrix();

        jointMatrixBuffer->UploadInline(
            encoder,
            skeleton
                | std::views::transform([](const Joint& j) { return j.globalTransform * j.inverseBindMatrix; })
                | std::ranges::to<std::vector>()
        );
    }
    else
    {
        jointMatrixBuffer->UploadInline(
            encoder,
            skeleton
                | std::views::transform([](const Joint& j) { return glm::identity<glm::mat4>(); })
                | std::ranges::to<std::vector>()
        );
    }
}

void Skin::Apply(ga::gpu::Device& gpu, const ga::gpu::CommandEncoder& encoder, ga::render::Mesh& mesh)
{
    std::swap(mesh.previousFramePositions, mesh.processedVertexBuffers[0]);;

    for (size_t i = 0; i < 4; ++i)
    {
        if (!mesh.vertexBuffers[i])
            continue;

        if (!mesh.processedVertexBuffers[i])
        {
            mesh.processedVertexBuffers[i] = std::make_unique<ga::gpu::Buffer>(gpu, ga::gpu::BufferInfo {
                .usage                   = ga::gpu::EBufferUsage::VertexBuffer | ga::gpu::EBufferUsage::StorageBuffer | (gpu.SupportsRaytracing() ? ga::gpu::EBufferUsage::AccelerationStructureBuildReadonly : gpu::EBufferUsage::None),
		        .size                    = mesh.vertexBuffers[i]->Size(),
		        .createPersistentStaging = true,
		        .enforceDeviceLocal      = true
            });
        }
    }

    encoder.Compute({ *ms_skinPipeline }, [&](const ga::gpu::ComputeEncoder& compute) {
        (*ms_skinPC)["inPositions"]  = mesh.vertexBuffers[0]->GetGpuAddress();
        (*ms_skinPC)["outPositions"] = mesh.processedVertexBuffers[0]->GetGpuAddress();

        if (mesh.vertexBuffers[1])
        {
            (*ms_skinPC)["inNormals"]  = mesh.vertexBuffers[1]->GetGpuAddress();
            (*ms_skinPC)["outNormals"] = mesh.processedVertexBuffers[1]->GetGpuAddress();
        }

        if (mesh.vertexBuffers[2])
        {
            (*ms_skinPC)["inUVs"]  = mesh.vertexBuffers[2]->GetGpuAddress();
            (*ms_skinPC)["outUVs"] = mesh.processedVertexBuffers[2]->GetGpuAddress();
        }

        if (mesh.vertexBuffers[3])
        {
            (*ms_skinPC)["inTangents"]  = mesh.vertexBuffers[3]->GetGpuAddress();
            (*ms_skinPC)["outTangents"] = mesh.processedVertexBuffers[3]->GetGpuAddress();
        }

        (*ms_skinPC)["joints"]      = jointMatrixBuffer->GetGpuAddress();
        (*ms_skinPC)["inJoints"]    = mesh.vertexBuffers[4]->GetGpuAddress();
        (*ms_skinPC)["inWeights"]   = mesh.vertexBuffers[5]->GetGpuAddress();
        (*ms_skinPC)["vertexCount"] = mesh.vertexCount;

        compute.PushConstants(*ms_skinPC);
        compute.Dispatch({ (mesh.vertexCount + 63) / 64, 1, 1 });
    });

    encoder.Transfer([&](const ga::gpu::TransferEncoder& transfer) {
        for (size_t i = 0; i < mesh.processedVertexBuffers.size(); ++i)
            if (mesh.processedVertexBuffers[i])
                transfer.BufferBarrier(*mesh.processedVertexBuffers[i]);
    });

    mesh.UpdateAccelerationStructures(gpu, &encoder);
}
