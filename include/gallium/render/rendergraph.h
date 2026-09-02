#ifndef GALLIUM__RENDER__RENDERGRAPHEX_H
#define GALLIUM__RENDER__RENDERGRAPHEX_H
#pragma once

#include <gallium/string_hash.h>
#include <gallium/gpu/device.h>
#include <gallium/gpu/commandbuffer.h>

#include <gallium/gpu/computepipeline.h>
#include <gallium/gpu/graphicspipeline.h>
#include <gallium/gpu/raytracingpipeline.h>

#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace ga::render
{
    using RenderExecuteFn   = std::function<void(const ga::gpu::RenderEncoder&)>;
    using ComputeExecuteFn  = std::function<void(const ga::gpu::ComputeEncoder&)>;
    using TransferExecuteFn = std::function<void(const ga::gpu::TransferEncoder&)>;
    using RaytraceExecuteFn = std::function<void(const ga::gpu::RaytracingEncoder&)>;
    using ManualCommandsFn  = std::function<void(const ga::gpu::CommandEncoder&)>;
    using ExecuteFn         = std::variant<RenderExecuteFn, ComputeExecuteFn, TransferExecuteFn, RaytraceExecuteFn>;

    enum class ERGBindingFlags
    {
        None      = 0,
        MatchSize = (1 << 0)
    };

    struct RGBinding
    {
        string_hash_t   id;
        ga::gpu::Image* image;
        ERGBindingFlags flag = ERGBindingFlags::None;
    };

    struct RGImageInfo
    {
        gpu::EFormat format   = gpu::EFormat::Undefined;
        glm::uvec2   size     = { 0, 0 };
        bool         external = false;

        bool operator==(const RGImageInfo&) const = default;
    };

    using RGPipeline = std::variant<
        std::unique_ptr<ga::gpu::GraphicsPipeline>,
        std::unique_ptr<ga::gpu::ComputePipeline>,
        std::unique_ptr<ga::gpu::RaytracingPipeline>,
        std::nullptr_t
    >;

    template<gpu::ELoadOp> struct RGLoadOp {};
    template<> struct RGLoadOp<gpu::ELoadOp::Clear> { glm::vec4 clearColor; float clearDepth; };

    class RenderGraph;

    class PassBuilder
    {
    public:
        struct Access
        {
            using ClearValue = std::variant<glm::vec4, float>;

            string_hash_t              id;
            ga::gpu::EImageUsage       usage;
            std::optional<RGImageInfo> info;
            bool                       isWrite;

            gpu::ELoadOp   loadOp     = gpu::ELoadOp::DontCare;
            gpu::EStoreOp  storeOp    = gpu::EStoreOp::Store;
            ClearValue     clearValue = glm::vec4(0.f, 0.f, 0.f, 1.f);
        };
 
    private:
        std::string                                          m_name;
        std::vector<Access>                                  m_accesses;
        std::vector<std::pair<string_hash_t, string_hash_t>> m_swaps;
        ExecuteFn                                            m_fn;
        ManualCommandsFn                                     m_preFn;
        ManualCommandsFn                                     m_postFn;
        std::function<bool()>                                m_condition;
        RGPipeline                                           m_pipeline = nullptr;
 
        friend class RenderGraph;

    public:
        PassBuilder& ReadsFrom(string_hash_t id, gpu::EImageUsage usage);
        PassBuilder& WritesTo(string_hash_t id, gpu::EImageUsage usage, const std::optional<RGImageInfo>& info = std::nullopt);
        PassBuilder& LoadOp(gpu::ELoadOp op);
        PassBuilder& LoadOp(gpu::ELoadOp op, glm::vec4 clearValue);
        PassBuilder& LoadOp(gpu::ELoadOp op, float clearValue);
        PassBuilder& StoreOp(gpu::EStoreOp op);
        PassBuilder& Render(RenderExecuteFn fn);
        PassBuilder& Compute(ComputeExecuteFn fn);
        PassBuilder& Raytrace(RaytraceExecuteFn fn);
        PassBuilder& Transfer(TransferExecuteFn fn);
        PassBuilder& Pre(ManualCommandsFn fn);
        PassBuilder& Post(ManualCommandsFn fn);
        PassBuilder& Swap(string_hash_t lhs, string_hash_t rhs);
        PassBuilder& If(std::function<bool()> condition);
    };

    class RenderGraph
    {
        struct Internal;
        std::unique_ptr<Internal> m_internal;

        std::string m_name = "RenderGraph";

    public:
        RenderGraph(ga::gpu::Device& gpu);
        ~RenderGraph();

        void SetName(const std::string& name);

        void Persist(string_hash_t id, const RGImageInfo& info);
        bool IsPersisted(string_hash_t id) const;
        PassBuilder& AddPass(const std::string& name, RGPipeline&& pipeline = nullptr);

        uint32_t    IndexOf(string_hash_t id) const;
        gpu::Image& ValueOf(string_hash_t id) const;
        glm::uvec2  OutputSize() const;

        void Compile();
        void Execute(const ga::gpu::CommandEncoder& encoder, const std::vector<RGBinding>& bindings);
    };
}

#endif /* GALLIUM__RENDER__RENDERGRAPHEX_H */
