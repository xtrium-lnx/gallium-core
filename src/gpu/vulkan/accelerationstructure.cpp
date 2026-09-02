#include "accelerationstructure_impl.h"
#include "buffer_impl.h"
#include "enums_impl.h"

#include <algorithm>
#include <ranges>

#include <glm/gtc/type_ptr.hpp>

using namespace ga::gpu;

std::pair<std::unique_ptr<Buffer>, vk::raii::AccelerationStructureKHR> s_BuildAccelerationStructure(
    Device& gpu,
    vk::AccelerationStructureTypeKHR type,
    const vk::AccelerationStructureGeometryKHR& geometry,
    const vk::AccelerationStructureBuildRangeInfoKHR& buildRange,
    vk::BuildAccelerationStructureFlagsKHR flags,
    const CommandEncoder* encoder = nullptr
) {
    const auto& device = gpu.GetImpl().device;

    auto alignUp = [](auto value, size_t alignment) noexcept { return ((value + alignment - 1) & ~(alignment - 1)); };

    auto buildInfo = vk::AccelerationStructureBuildGeometryInfoKHR {
        .type          = type,
        .flags         = flags,
        .mode          = vk::BuildAccelerationStructureModeKHR::eBuild,
        .geometryCount = 1,
        .pGeometries   = &geometry,
    };

    auto buildSize = device.getAccelerationStructureBuildSizesKHR(vk::AccelerationStructureBuildTypeKHR::eDevice, buildInfo, { buildRange.primitiveCount });
    vk::DeviceSize scratchSize = alignUp(buildSize.buildScratchSize, gpu.GetImpl().deviceCaps.asProperties.minAccelerationStructureScratchOffsetAlignment);

    auto scratchBuffer = std::make_unique<Buffer>(gpu, BufferInfo {
        .usage              = EBufferUsage::StorageBuffer | EBufferUsage::AccelerationStructureStorage,
        .size               = scratchSize,
        .enforceDeviceLocal = true
    });

    auto asBuffer = std::make_unique<Buffer>(gpu, BufferInfo {
        .usage              = EBufferUsage::AccelerationStructureStorage | EBufferUsage::AccelerationStructureBuildReadonly,
        .size               = buildSize.accelerationStructureSize,
        .enforceDeviceLocal = true
    });

    auto createInfo = vk::AccelerationStructureCreateInfoKHR {
        .buffer = asBuffer->GetImpl().buffer,
        .size   = buildSize.accelerationStructureSize,
        .type   = type
    };
    auto result = device.createAccelerationStructureKHR(createInfo);

    buildInfo.dstAccelerationStructure  = result;
    buildInfo.scratchData.deviceAddress = scratchBuffer->GetGpuAddress();

    if (!encoder)
    {
        auto* commandBuffer = gpu.AcquireCommandBuffer();
        commandBuffer->Record([&](const ga::gpu::CommandEncoder&) {
            commandBuffer->GetImpl().commandBuffer.buildAccelerationStructuresKHR({ buildInfo }, { &buildRange });
        });
        gpu.SubmitAndWait(commandBuffer);
        gpu.ReleaseCommandBuffer(commandBuffer);
    }
    else
    {
        encoder->GetImpl().commandBuffer->buildAccelerationStructuresKHR({ buildInfo }, { &buildRange });
        encoder->Transfer([](const ga::gpu::TransferEncoder& transfer) { transfer.ASBarrier(); });
    }

    return { std::move(asBuffer), std::move(result) };
}

ASGeometry* AccelerationStructure::CreateGeometry(Device& gpu, const ASGeometryInfo& info)
{
    auto vkFormat = s_ToVk(info.vertexFormat);
    auto stride   = s_ToVkComponentSize(vkFormat) * s_ToVkComponentCount(vkFormat);
    auto vbAddr   = info.vertexBuffer->GetGpuAddress();
    auto ibAddr   = info.indexBuffer->GetGpuAddress();

    auto geometry = vk::AccelerationStructureGeometryKHR
    {
        .geometryType = vk::GeometryTypeKHR::eTriangles,
        .geometry     = vk::AccelerationStructureGeometryTrianglesDataKHR {
            .vertexFormat = s_ToVk(info.vertexFormat),
            .vertexData   = vbAddr + info.firstVertex,
            .vertexStride = stride,
            .maxVertex    = uint32_t(info.vertexBuffer->GetImpl().size / stride),
            .indexType    = vk::IndexType::eUint32,
            .indexData    = ibAddr + info.firstIndex * sizeof(uint32_t),
        },
        .flags = vk::GeometryFlagBitsKHR::eNoDuplicateAnyHitInvocation | vk::GeometryFlagBitsKHR::eOpaque,
    };

    auto buildRange = vk::AccelerationStructureBuildRangeInfoKHR { .primitiveCount = uint32_t(info.indexCount / 3) };

    auto result = std::make_unique<ASGeometry>().release();

    std::tie(result->blasBuffer, result->blas) = s_BuildAccelerationStructure(gpu,
        vk::AccelerationStructureTypeKHR::eBottomLevel,
        geometry, buildRange,
        vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace
    );

    result->blasGpuAddress = gpu.GetImpl().device.getAccelerationStructureAddressKHR({
        .accelerationStructure = result->blas
    });

    return result;
}

ASGeometry* AccelerationStructure::CreateGeometryInline(Device& gpu, const CommandEncoder& encoder, const ASGeometryInfo& info)
{
    auto vkFormat = s_ToVk(info.vertexFormat);
    auto stride   = s_ToVkComponentSize(vkFormat) * s_ToVkComponentCount(vkFormat);
    auto vbAddr   = info.vertexBuffer->GetGpuAddress();
    auto ibAddr   = info.indexBuffer->GetGpuAddress();

    auto geometry = vk::AccelerationStructureGeometryKHR
    {
        .geometryType = vk::GeometryTypeKHR::eTriangles,
        .geometry     = vk::AccelerationStructureGeometryTrianglesDataKHR {
            .vertexFormat = s_ToVk(info.vertexFormat),
            .vertexData   = vbAddr + info.firstVertex,
            .vertexStride = stride,
            .maxVertex    = uint32_t(info.vertexBuffer->GetImpl().size / stride),
            .indexType    = vk::IndexType::eUint32,
            .indexData    = ibAddr + info.firstIndex * sizeof(uint32_t),
        },
        .flags = vk::GeometryFlagBitsKHR::eNoDuplicateAnyHitInvocation | vk::GeometryFlagBitsKHR::eOpaque,
    };

    auto buildRange = vk::AccelerationStructureBuildRangeInfoKHR { .primitiveCount = uint32_t(info.indexCount / 3) };

    auto result = std::make_unique<ASGeometry>().release();

    std::tie(result->blasBuffer, result->blas) = s_BuildAccelerationStructure(gpu,
        vk::AccelerationStructureTypeKHR::eBottomLevel,
        geometry, buildRange,
        vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace, &encoder
    );

    result->blasGpuAddress = gpu.GetImpl().device.getAccelerationStructureAddressKHR({
        .accelerationStructure = result->blas
    });

    return result;
}

ASGeometry* AccelerationStructure::CreateAabb(Device& gpu, const std::array<glm::vec3, 2>& aabbMinMax)
{
    std::array aabb = { vk::AabbPositionsKHR {
        aabbMinMax[0].x, aabbMinMax[0].y, aabbMinMax[0].z,
        aabbMinMax[1].x, aabbMinMax[1].y, aabbMinMax[1].z
    } };

    auto aabbBuffer = std::make_unique<ga::gpu::Buffer>(gpu, ga::gpu::EBufferUsage::AccelerationStructureBuildReadonly, aabb);

    auto geometry = vk::AccelerationStructureGeometryKHR
    {
        .geometryType = vk::GeometryTypeKHR::eAabbs,
        .geometry = vk::AccelerationStructureGeometryAabbsDataKHR {
            .data = aabbBuffer->GetGpuAddress(),
            .stride = sizeof(vk::AabbPositionsKHR)
        },
        .flags = vk::GeometryFlagBitsKHR::eNoDuplicateAnyHitInvocation | vk::GeometryFlagBitsKHR::eOpaque,
    };

    auto buildRange = vk::AccelerationStructureBuildRangeInfoKHR{ .primitiveCount = 1u };

    auto result = std::make_unique<ASGeometry>().release();

    std::tie(result->blasBuffer, result->blas) = s_BuildAccelerationStructure(gpu,
        vk::AccelerationStructureTypeKHR::eBottomLevel,
        geometry, buildRange,
        vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace
    );

    result->blasGpuAddress = gpu.GetImpl().device.getAccelerationStructureAddressKHR({
        .accelerationStructure = result->blas
    });

    return result;
}

ASGeometry* AccelerationStructure::CreateAabbInline(Device& gpu, const CommandEncoder& encoder, const std::array<glm::vec3, 2>& aabbMinMax)
{
    std::array aabb = { vk::AabbPositionsKHR {
        aabbMinMax[0].x, aabbMinMax[0].y, aabbMinMax[0].z,
        aabbMinMax[1].x, aabbMinMax[1].y, aabbMinMax[1].z
    } };

    auto aabbBuffer = std::make_unique<ga::gpu::Buffer>(gpu, ga::gpu::BufferInfo{
        .usage = ga::gpu::EBufferUsage::AccelerationStructureBuildReadonly,
        .size  = sizeof(vk::AabbPositionsKHR),
        .createPersistentStaging = true
    });
    encoder.Transfer([&](const ga::gpu::TransferEncoder& transfer) { aabbBuffer->UploadInline(transfer, aabb); });
    
    auto geometry = vk::AccelerationStructureGeometryKHR
    {
        .geometryType = vk::GeometryTypeKHR::eAabbs,
        .geometry = vk::AccelerationStructureGeometryAabbsDataKHR {
            .data = aabbBuffer->GetGpuAddress(),
            .stride = sizeof(vk::AabbPositionsKHR)
        },
        .flags = vk::GeometryFlagBitsKHR::eNoDuplicateAnyHitInvocation | vk::GeometryFlagBitsKHR::eOpaque,
    };

    auto buildRange = vk::AccelerationStructureBuildRangeInfoKHR{ .primitiveCount = 1u };

    auto result = std::make_unique<ASGeometry>().release();

    std::tie(result->blasBuffer, result->blas) = s_BuildAccelerationStructure(gpu,
        vk::AccelerationStructureTypeKHR::eBottomLevel,
        geometry, buildRange,
        vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace,
        &encoder
    );

    result->blasGpuAddress = gpu.GetImpl().device.getAccelerationStructureAddressKHR({
        .accelerationStructure = result->blas
    });

    gpu.Defer([b = std::move(aabbBuffer)] {});
    return result;
}

void AccelerationStructure::DestroyGeometry(ASGeometry*& geometry)
{
    if (!geometry)
        return;

    geometry->blas.clear();
    geometry->blasBuffer.reset();

    delete geometry;
    geometry = nullptr;
}

// ----------------------------------------------------------------------------

AccelerationStructure::AccelerationStructure(Device& gpu, const std::vector<ASGeometryInstance>& instances, const ASProceduralInjectorList& proceduralInjectors /* = {} */)
    : m_pImpl(new Impl)
{
    const auto& device = gpu.GetImpl().device;

    auto toVkTransformMatrixKHR = [](const glm::mat4& m)
    {
        vk::TransformMatrixKHR t;
        memcpy(&t, glm::value_ptr(glm::transpose(m)), sizeof(t));
        return t;
    };

    size_t numProceduralInstances = 0;
    for (const auto& [count, _] : proceduralInjectors)
        numProceduralInstances += count;

    std::vector<vk::AccelerationStructureInstanceKHR> tlasInstances;
    tlasInstances.reserve(instances.size() + numProceduralInstances);

    for (const auto& instance : instances)
    {
        tlasInstances.push_back(vk::AccelerationStructureInstanceKHR {
            .transform                      = toVkTransformMatrixKHR(instance.transform),
            .instanceCustomIndex            = instance.customValue,
            .mask                           = uint32_t(instance.mask),
            .accelerationStructureReference = instance.geometry->blasGpuAddress
        });
    }

    m_pImpl->instanceBuffer = std::make_unique<Buffer>(gpu, EBufferUsage::AccelerationStructureBuildReadonly, tlasInstances);

    auto* cb = gpu.AcquireCommandBuffer();
    cb->Record([&](const CommandEncoder& encoder) {
        size_t instanceOffset = instances.size();
        for (const auto& [count, injector] : proceduralInjectors)
        {
            injector(*m_pImpl->instanceBuffer, instanceOffset, encoder);
            instanceOffset += count;
        }
    }, ECommandBufferUsage::OneTimeSubmit);
    gpu.SubmitAndWait(cb);
    gpu.ReleaseCommandBuffer(cb);

    auto tlasGeometryInstances = vk::AccelerationStructureGeometryInstancesDataKHR {
        .data = m_pImpl->instanceBuffer->GetGpuAddress()
    };

    auto tlasGeometry = vk::AccelerationStructureGeometryKHR {
        .geometryType = vk::GeometryTypeKHR::eInstances,
        .geometry     = tlasGeometryInstances
    };

    auto buildRangeInfo = vk::AccelerationStructureBuildRangeInfoKHR {
        .primitiveCount = uint32_t(instances.size())
    };

    std::tie(m_pImpl->tlasBuffer, m_pImpl->tlas) = s_BuildAccelerationStructure(
        gpu,
        vk::AccelerationStructureTypeKHR::eTopLevel,
        tlasGeometry,
        buildRangeInfo,
        vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace
    );

    m_pImpl->gpuAddress = gpu.GetImpl().device.getAccelerationStructureAddressKHR({
        .accelerationStructure = m_pImpl->tlas
    });
}

AccelerationStructure::AccelerationStructure(Device& gpu, const CommandEncoder& encoder, const std::vector<ASGeometryInstance>& instances, const ASProceduralInjectorList& proceduralInjectors /* = {} */)
    : m_pImpl(new Impl)
{
    const auto& device = gpu.GetImpl().device;

    auto toVkTransformMatrixKHR = [](const glm::mat4& m)
    {
        vk::TransformMatrixKHR t;
        memcpy(&t, glm::value_ptr(glm::transpose(m)), sizeof(t));
        return t;
    };

    size_t numProceduralInstances = 0;
    for (const auto& [count, _] : proceduralInjectors)
        numProceduralInstances += count;

    std::vector<vk::AccelerationStructureInstanceKHR> tlasInstances;
    tlasInstances.reserve(instances.size() + numProceduralInstances);

    for (const auto& instance : instances)
    {
        tlasInstances.push_back(vk::AccelerationStructureInstanceKHR {
            .transform                              = toVkTransformMatrixKHR(instance.transform),
            .instanceCustomIndex                    = instance.customValue,
            .mask                                   = uint32_t(instance.mask),
            .instanceShaderBindingTableRecordOffset = instance.hitGroup,
            .accelerationStructureReference         = instance.geometry->blasGpuAddress
        });
    }

    m_pImpl->instanceBuffer = std::make_unique<Buffer>(gpu, EBufferUsage::AccelerationStructureBuildReadonly, tlasInstances, true);
    encoder.Transfer([&](const TransferEncoder& transfer) {
        m_pImpl->instanceBuffer->UploadInline(transfer, tlasInstances);
    });

    size_t instanceOffset = instances.size();
    for (const auto& [count, injector] : proceduralInjectors)
    {
        injector(*m_pImpl->instanceBuffer, instanceOffset, encoder);
        instanceOffset += count;
    }

    auto tlasGeometryInstances = vk::AccelerationStructureGeometryInstancesDataKHR {
        .data = m_pImpl->instanceBuffer->GetGpuAddress()
    };

    auto tlasGeometry = vk::AccelerationStructureGeometryKHR {
        .geometryType = vk::GeometryTypeKHR::eInstances,
        .geometry = tlasGeometryInstances
    };

    auto buildRangeInfo = vk::AccelerationStructureBuildRangeInfoKHR{
        .primitiveCount = uint32_t(instances.size())
    };

    std::tie(m_pImpl->tlasBuffer, m_pImpl->tlas) = s_BuildAccelerationStructure(
        gpu,
        vk::AccelerationStructureTypeKHR::eTopLevel,
        tlasGeometry,
        buildRangeInfo,
        vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace,
        &encoder
    );

    m_pImpl->gpuAddress = gpu.GetImpl().device.getAccelerationStructureAddressKHR({
        .accelerationStructure = m_pImpl->tlas
    });
}

AccelerationStructure::~AccelerationStructure()
{
    m_pImpl->tlas.clear();
    m_pImpl->tlasBuffer.reset();
}

AccelerationStructure::Impl& AccelerationStructure::GetImpl()
{
    return *m_pImpl;
}

uintptr_t AccelerationStructure::GetGpuAddress() const
{
    return m_pImpl->gpuAddress;
}

// ----------------------------------------------------------------------------

AccelerationStructureBuilder::AccelerationStructureBuilder(Device& gpu)
    : m_gpu(gpu)
{
}

AccelerationStructureBuilder& AccelerationStructureBuilder::AddInstance(const ASGeometryInstance& instance)
{
    m_instances.push_back(instance);
    return *this;
}

AccelerationStructureBuilder& AccelerationStructureBuilder::AddInstances(const std::vector<ASGeometryInstance>& instances)
{
    for (const auto& instance : instances)
        m_instances.push_back(instance);

    return *this;
}

std::unique_ptr<AccelerationStructure> AccelerationStructureBuilder::Build()
{
    return std::make_unique<AccelerationStructure>(m_gpu, m_instances);
}

std::unique_ptr<AccelerationStructure> AccelerationStructureBuilder::BuildInline(const CommandEncoder& encoder)
{
    return std::make_unique<AccelerationStructure>(m_gpu, encoder, m_instances);
}
