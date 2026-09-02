#include <gallium/render/rendergraph.h>
#include <gallium/gpu/descriptorregistry.h>
#include <gallium/gpu/image.h>

#include <format>
#include <stdexcept>

// #define ENABLE_RENDERGRAPH_DUMP
#ifdef ENABLE_RENDERGRAPH_DUMP
# include <print>
#endif /* ENABLE_RENDERGRAPH_DUMP */

using namespace ga::render;

class ResourcePool
{
	struct Key
    {
        ga::gpu::EImageUsage usage;
        ga::gpu::EFormat     format;
        glm::uvec2           size;

		bool IsCompatibleWith(const Key & o) const
		{
			return format == o.format
				&& size == o.size
				&& (usage & o.usage) == o.usage; // AT LEAST o's usage
		}
    };

    struct Slot
    {
        Key                              key;
        std::unique_ptr<ga::gpu::Image>  image;
        uint32_t                         age = 0;
    };

    ga::gpu::Device&   m_device;
    std::vector<Slot>  m_live;
    std::vector<Slot>  m_free;

public:
    explicit ResourcePool(ga::gpu::Device& device)
		: m_device(device)
	{}

    ga::gpu::Image* Acquire(ga::gpu::EImageUsage usage, ga::gpu::EFormat format, glm::uvec2 size)
    {
		auto supersetUsage = m_SupersetUsage(usage, format);
        const Key key { supersetUsage, format, size };

        for (auto it = m_free.begin(); it != m_free.end(); ++it)
        {
            if (it->key.IsCompatibleWith(key))
            {
                ga::gpu::Image* img = it->image.get();
                m_live.push_back(std::move(*it));
                m_free.erase(it);
                return img;
            }
        }

        auto image = std::make_unique<ga::gpu::Image>(m_device, ga::gpu::ImageInfo {
            .type         = ga::gpu::EImageType::Image2D,
            .format       = format,
            .width        = size.x,
            .height       = size.y,
            .usage        = supersetUsage,
			.samplingMode = ga::gpu::EImageSamplingMode::ClampToEdge
        });
		image->SetDebugName("RenderGraph::ResourcePool image");

		if (((supersetUsage & ga::gpu::EImageUsage::Sampled) == ga::gpu::EImageUsage::Sampled) ||
			((supersetUsage & ga::gpu::EImageUsage::Storage) == ga::gpu::EImageUsage::Storage))
			image->SetDescriptorIndex(m_device.GetDescriptorRegistry().Register(*image));

        ga::gpu::Image* raw = image.get();
        m_live.push_back({ key, std::move(image) });

        return raw;
    }

    void Release(ga::gpu::Image* image)
    {
        auto it = std::find_if(m_live.begin(), m_live.end(), [image](const Slot& s) { return s.image.get() == image; });

        if (it != m_live.end())
        {
            it->age = 0;
            m_free.push_back(std::move(*it));
            m_live.erase(it);
        }
    }

    void Trim(uint32_t maxAge = 4)
    {
        for (auto& slot : m_free)
			slot.age++;

		m_free.erase(std::remove_if(m_free.begin(), m_free.end(), [&](const Slot& s) {
			if (s.age >= maxAge)
			{
				auto descriptorIndex = s.image->GetDescriptorIndex();
				s.image->SetDescriptorIndex({});
				m_device.GetDescriptorRegistry().Unregister(descriptorIndex);
				return true;
			}
			return false;
		}), m_free.end());
    }

    size_t LiveCount() const { return m_live.size(); }
    size_t FreeCount() const { return m_free.size(); }

private:
    static ga::gpu::EImageUsage m_SupersetUsage(ga::gpu::EImageUsage usage, ga::gpu::EFormat format)
    {
        if (((usage & ga::gpu::EImageUsage::DepthStencil) != ga::gpu::EImageUsage::None) || format == ga::gpu::EFormat::D32_SFloat)
        {
            return ga::gpu::EImageUsage::DepthStencil
                    | ga::gpu::EImageUsage::Sampled
                    | ga::gpu::EImageUsage::TransferSrc
					| ga::gpu::EImageUsage::TransferDst;
        }

        return ga::gpu::EImageUsage::ColorAttachment
                | ga::gpu::EImageUsage::Storage
                | ga::gpu::EImageUsage::Sampled
                | ga::gpu::EImageUsage::TransferSrc
                | ga::gpu::EImageUsage::TransferDst;
    }
};

// ----------------------------------------------------------------------------

struct PersistentImage
{
    ga::gpu::Image*      image       = nullptr;
    ga::gpu::EImageUsage usage       = ga::gpu::EImageUsage::None;
    ga::gpu::EFormat     format;
    glm::uvec2           size        = { 0, 0 };
    glm::uvec2           currentSize = { 0, 0 };

    void Update(ResourcePool& pool, glm::uvec2 newSize)
    {
        if (image && currentSize == newSize)
            return;

		Release(pool);
        image = pool.Acquire(usage, format, newSize);
		currentSize = newSize;
    }

    void Release(ResourcePool& pool)
    {
        if (image)
		{
			pool.Release(image);
			image = nullptr;
		}
    }
};

// ----------------------------------------------------------------------------

PassBuilder& PassBuilder::ReadsFrom(string_hash_t id, gpu::EImageUsage usage)
{
	m_accesses.push_back({ id, usage, std::nullopt, false });
	return *this;
}

PassBuilder& PassBuilder::WritesTo(string_hash_t id, gpu::EImageUsage usage, const std::optional<RGImageInfo>& info /* = std::nullopt */)
{
	m_accesses.push_back({ id, usage, info, true });
	return *this;
}

PassBuilder& PassBuilder::LoadOp(gpu::ELoadOp op)
{
	auto& last = m_accesses.back();
	last.loadOp = op;
	return *this;
}

PassBuilder& PassBuilder::LoadOp(gpu::ELoadOp op, glm::vec4 clearValue)
{
	auto& last = m_accesses.back();
	last.loadOp = op;
	last.clearValue = clearValue;
	return *this;
}

PassBuilder& PassBuilder::LoadOp(gpu::ELoadOp op, float clearValue)
{
	auto& last = m_accesses.back();
	last.loadOp = op;
	last.clearValue = clearValue;
	return *this;
}

PassBuilder& PassBuilder::StoreOp(gpu::EStoreOp op)
{
	m_accesses.back().storeOp = op;
	return *this;
}

PassBuilder& PassBuilder::Render(RenderExecuteFn fn)
{
	if (!std::holds_alternative<std::unique_ptr<ga::gpu::GraphicsPipeline>>(m_pipeline))
		throw std::runtime_error("Trying to call Render on a non-graphics pipeline");

	m_fn = std::move(fn);
	return *this;
}

PassBuilder& PassBuilder::Compute(ComputeExecuteFn fn)
{
	if (!std::holds_alternative<std::unique_ptr<ga::gpu::ComputePipeline>>(m_pipeline))
		throw std::runtime_error("Trying to call Render on a non-graphics pipeline");

	m_fn = std::move(fn);
	return *this;
}

PassBuilder& PassBuilder::Raytrace(RaytraceExecuteFn fn)
{
	if (!std::holds_alternative<std::unique_ptr<ga::gpu::RaytracingPipeline>>(m_pipeline))
		throw std::runtime_error("Trying to call Raytrace on a non-raytracing pipeline");

	m_fn = std::move(fn);
	return *this;
}

PassBuilder& PassBuilder::Transfer(TransferExecuteFn fn)
{
	if (!std::holds_alternative<std::nullptr_t>(m_pipeline))
		throw std::runtime_error("Trying to call Transfer with a set pipeline");

	m_fn = std::move(fn);
	return *this;
}

PassBuilder& PassBuilder::Pre(ManualCommandsFn fn)
{
	m_preFn = std::move(fn);
	return *this;
}

PassBuilder& PassBuilder::Post(ManualCommandsFn fn)
{
	m_postFn = std::move(fn);
	return *this;
}

PassBuilder& PassBuilder::Swap(string_hash_t lhs, string_hash_t rhs)
{
	m_swaps.push_back({ lhs, rhs });
	return *this;
}

PassBuilder& PassBuilder::If(std::function<bool()> condition)
{
	m_condition = std::move(condition);
	return *this;
}

// ----------------------------------------------------------------------------

struct ResolvedAcquire
{
	string_hash_t        id;
	ga::gpu::EImageUsage usage;
	ga::gpu::EFormat     format;
	glm::uvec2           size;
};

struct ResolvedTransition
{
	string_hash_t           id;
	ga::gpu::TransitionInfo info;
};

struct ResolvedRelease
{
	string_hash_t id;
};

struct ResolvedAttachment
{
	string_hash_t                   id;
	ga::gpu::ELoadOp                loadOp;
	ga::gpu::EStoreOp               storeOp;
	PassBuilder::Access::ClearValue clearValue;
};

struct CompiledPass
{
	std::string                                          name;
	std::vector<ResolvedAcquire>                         acquires;
	std::vector<ResolvedTransition>                      transitions;
	std::vector<ResolvedRelease>                         releases;
	std::vector<std::pair<string_hash_t, string_hash_t>> swaps;
	std::vector<ResolvedAttachment>                      colorAttachments;
	std::optional<ResolvedAttachment>                    depthAttachment;
	RGPipeline                                           pipeline;
	ExecuteFn                                            executeFn;
	ManualCommandsFn                                     preFn;
	ManualCommandsFn                                     postFn;
	std::function<bool()>                                condition;
};

using ImageDictionary = std::unordered_map<string_hash_t, ga::gpu::Image*>;

// ----------------------------------------------------------------------------

ga::gpu::EImageLayout s_UsageToLayout(ga::gpu::EImageUsage usage)
{
	if ((usage & ga::gpu::EImageUsage::DepthStencil) != ga::gpu::EImageUsage::None)
		return ga::gpu::EImageLayout::DepthAttachmentOptimal;

	if ((usage & ga::gpu::EImageUsage::ColorAttachment) != ga::gpu::EImageUsage::None)
		return ga::gpu::EImageLayout::ColorAttachmentOptimal;

	if ((usage & ga::gpu::EImageUsage::Storage) != ga::gpu::EImageUsage::None)
		return ga::gpu::EImageLayout::General;

	if ((usage & ga::gpu::EImageUsage::Sampled) != ga::gpu::EImageUsage::None)
		return ga::gpu::EImageLayout::ShaderReadOnlyOptimal;

	if ((usage & ga::gpu::EImageUsage::TransferSrc) != ga::gpu::EImageUsage::None)
		return ga::gpu::EImageLayout::TransferSrcOptimal;

	if ((usage & ga::gpu::EImageUsage::TransferDst) != ga::gpu::EImageUsage::None)
		return ga::gpu::EImageLayout::TransferDstOptimal;

	return ga::gpu::EImageLayout::Undefined;
}

enum class EPassType
{
	Render,
	Compute,
	Raytracing,
	Transfer,
	Unknown
};

std::tuple<ga::gpu::EAccessType, ga::gpu::EPipelineStage, ga::gpu::EImageLayout>
s_GetBarrierInfo(ga::gpu::EImageUsage usage, bool isWrite, EPassType passType)
{
	using namespace ga::gpu;

	EAccessType    access = EAccessType::None;
	EPipelineStage stage  = EPipelineStage::None;
	EImageLayout   layout = EImageLayout::Undefined;

	// Helper: resolve shader stage based on pass type
	auto shaderStage = [&]() -> EPipelineStage
	{
		switch (passType)
		{
		case EPassType::Render:     return EPipelineStage::FragmentShader;
		case EPassType::Compute:    return EPipelineStage::ComputeShader;
		case EPassType::Raytracing: return EPipelineStage::RaytracingShader;
		case EPassType::Transfer:   return EPipelineStage::Transfer;
		default:                    break;
		}
		return EPipelineStage::FragmentShader;
	};

	if ((usage & EImageUsage::ColorAttachment) != EImageUsage::None)
	{
		stage = stage | EPipelineStage::ColorAttachmentOutput;

		if (isWrite)
		{
			access = access | EAccessType::ColorAttachmentWrite;
			layout = EImageLayout::ColorAttachmentOptimal;
		}
		else
		{
			access = access | EAccessType::ColorAttachmentRead;
			layout = EImageLayout::ColorAttachmentOptimal;
		}
	}

	if ((usage & EImageUsage::DepthStencil) != EImageUsage::None)
	{
		stage = stage | EPipelineStage::EarlyFragmentTests | EPipelineStage::LateFragmentTests;

		if (isWrite)
		{
			access = access | EAccessType::DepthStencilAttachmentWrite;
			layout = EImageLayout::DepthAttachmentOptimal;
		}
		else
		{
			access = access | EAccessType::DepthStencilAttachmentRead;
			layout = EImageLayout::DepthReadOnlyOptimal;
		}
	}

	if ((usage & EImageUsage::Sampled) != EImageUsage::None)
	{
		stage = stage | shaderStage();
		access = access | EAccessType::ShaderRead;
		layout = EImageLayout::ShaderReadOnlyOptimal;
	}

	if ((usage & EImageUsage::Storage) != EImageUsage::None)
	{
		stage = stage | shaderStage();

		if (isWrite)
			access = access | EAccessType::ShaderWrite;
		else
			access = access | EAccessType::ShaderRead;

		layout = EImageLayout::General;
	}

	if ((usage & EImageUsage::TransferSrc) != EImageUsage::None)
	{
		stage  = stage | EPipelineStage::Transfer;
		access = access | EAccessType::TransferRead;
		layout = EImageLayout::TransferSrcOptimal;
	}

	if ((usage & EImageUsage::TransferDst) != EImageUsage::None)
	{
		stage  = stage | EPipelineStage::Transfer;
		access = access | EAccessType::TransferWrite;
		layout = EImageLayout::TransferDstOptimal;
	}

	if (stage == EPipelineStage::None)
		stage = EPipelineStage::TopOfPipe;

	if (layout == EImageLayout::Undefined)
		layout = EImageLayout::General;

	return { access, stage, layout };
}

bool s_IsAttachmentLayout(ga::gpu::EImageLayout layout)
{
	return layout == ga::gpu::EImageLayout::ColorAttachmentOptimal
		|| layout == ga::gpu::EImageLayout::DepthAttachmentOptimal;
}

ga::gpu::BeginRenderingInfo s_BuildBeginRenderingInfo(const CompiledPass& pass, ga::gpu::GraphicsPipeline& pipeline, const ImageDictionary& activeImages)
{
	ga::gpu::BeginRenderingInfo info {
		.pipeline   = pipeline,
		.renderArea = {.offset = { 0, 0 }, .extent = { UINT32_MAX, UINT32_MAX } },
		.layerCount = 1
	};

	for (auto attachment : pass.colorAttachments)
    {
        ga::gpu::Image* img = activeImages.at(attachment.id);
        info.renderArea.extent = glm::min(info.renderArea.extent, glm::uvec2(img->Size()));
        info.colorAttachments.push_back({
            .image      = *img,
            .loadOp     = attachment.loadOp,
			.storeOp    = attachment.storeOp,
        });

		if (info.colorAttachments.back().loadOp == ga::gpu::ELoadOp::Clear)
			info.colorAttachments.back().clearValue = std::get<glm::vec4>(attachment.clearValue);
    }

    if (pass.depthAttachment)
    {
        ga::gpu::Image* img = activeImages.at(pass.depthAttachment->id);
        info.renderArea.extent = glm::min(info.renderArea.extent, glm::uvec2(img->Size()));
        info.depthAttachment.emplace(ga::gpu::BeginRenderingInfo::DepthAttachment {
            .image      = *img,
            .loadOp     = pass.depthAttachment->loadOp,
            .storeOp    = pass.depthAttachment->storeOp,
        });

		if (info.depthAttachment->loadOp == ga::gpu::ELoadOp::Clear)
			info.depthAttachment->clearValue = std::get<float>(pass.depthAttachment->clearValue);
    }

	return info;
}

// ----------------------------------------------------------------------------

struct RenderGraph::Internal
{
	std::unique_ptr<ResourcePool>                       resourcePool;
	std::vector<PassBuilder>                            passBuilders;
	std::vector<CompiledPass>                           passes;

	std::unordered_map<string_hash_t, PersistentImage>  persistentImages;

	ImageDictionary                                     activeImages;
	glm::uvec2                                          outputSize;

	uint32_t MatchOutputCoordinate(uint32_t v, uint32_t o)
	{
		if (!v)
			return o;

		if (!(v & 0x80000000))
			return v;

		uint32_t shift = glm::min(31u, (~v + 1));
		return glm::max(1u, o >> shift);
	}

	glm::uvec2 MatchOutputSize(glm::uvec2& size)
	{
		return glm::uvec2 {
			MatchOutputCoordinate(size.x, outputSize.x),
			MatchOutputCoordinate(size.y, outputSize.y)
		};
	}
};

RenderGraph::RenderGraph(ga::gpu::Device& gpu)
	: m_internal(new Internal)
{
	m_internal->resourcePool = std::make_unique<ResourcePool>(gpu);
	m_internal->outputSize = glm::uvec2(0);
}

RenderGraph::~RenderGraph()
{
}

void RenderGraph::Persist(string_hash_t id, const RGImageInfo& info)
{
	m_internal->persistentImages[id] = PersistentImage {
		.format = info.format,
		.size   = info.size
	};
}

bool RenderGraph::IsPersisted(string_hash_t id) const
{
	return m_internal->persistentImages.contains(id) && m_internal->activeImages.contains(id);
}

void RenderGraph::SetName(const std::string& name)
{
	m_name = name;
}

PassBuilder& RenderGraph::AddPass(const std::string& name, RGPipeline&& pipeline /* = nullptr */)
{
	m_internal->passBuilders.emplace_back();
	m_internal->passBuilders.back().m_pipeline = std::move(pipeline);
	m_internal->passBuilders.back().m_name     = name;
	return m_internal->passBuilders.back();
}

uint32_t RenderGraph::IndexOf(string_hash_t id) const
{
	if (!m_internal->activeImages.contains(id))
		return UINT32_MAX;

	return m_internal->activeImages[id]->GetDescriptorIndex().slot;
}

ga::gpu::Image& RenderGraph::ValueOf(string_hash_t id) const
{
	if (!m_internal->activeImages.contains(id))
		throw std::runtime_error("Unknown resource ID");

	return *m_internal->activeImages[id];
}

glm::uvec2 RenderGraph::OutputSize() const
{
	return m_internal->outputSize;
}

void RenderGraph::Compile()
{
	if (m_internal->passBuilders.empty())
		return; // Render graph is empty

	std::vector<std::vector<string_hash_t>> acquireList(m_internal->passBuilders.size());
	std::vector<std::vector<string_hash_t>> releaseList(m_internal->passBuilders.size());

	struct ResourceMeta
	{
		size_t                     firstPass      = -1;
		size_t                     lastPass       = -1;
		std::optional<RGImageInfo> info           = std::nullopt;
		ga::gpu::EImageUsage       combinedUsage  = ga::gpu::EImageUsage::None;
		bool                       writtenByGraph = false;
	};

	std::unordered_map<string_hash_t, ResourceMeta> resourceMetas;

	for (size_t i = 0; i < m_internal->passBuilders.size(); ++i)
	{
		for (auto& acc : m_internal->passBuilders[i].m_accesses)
		{
			auto& m = resourceMetas[acc.id];
			
			if (!m_internal->persistentImages.contains(acc.id))
			{
				if (m.firstPass == -1)
					m.firstPass = i;

				m.lastPass = i;
			}

			m.combinedUsage = m.combinedUsage | acc.usage;

			if (acc.isWrite)
				m.writtenByGraph = true;

			if (acc.info)
			{
				if (m.info)
				{
					if (*m.info != *acc.info)
						throw std::runtime_error("Image format/size mismatch");
				}
				else
					m.info = acc.info;
			}
		}
	}

	std::unordered_map<string_hash_t, PersistentImage> actualPersistentImages;

	for (auto& [id, pi] : m_internal->persistentImages)
	{
		if (!resourceMetas.contains(id))
			continue;

		pi.usage = resourceMetas[id].combinedUsage;
		actualPersistentImages[id] = pi;
	}

	m_internal->persistentImages = actualPersistentImages;

	for (auto& [id, m] : resourceMetas)
	{
		if (!m.writtenByGraph)
			continue;

		if (m_internal->persistentImages.contains(id))
			continue;

		if (!m.info)
			throw std::runtime_error("Transient image has no info");

		if (!m.info->external)
		{
			if (m.firstPass != uint32_t(-1) && m.lastPass != uint32_t(-1))
			{
				acquireList[m.firstPass].push_back(id);
				releaseList[m.lastPass].push_back(id);
			}
		}
	}

	struct ImageState
	{
		gpu::EImageUsage  usage;
		gpu::EImageLayout layout;
		bool              isWrite;
		EPassType         passType;
	};
	std::unordered_map<string_hash_t, ImageState> currentStates;

	for (size_t i = 0; i < m_internal->passBuilders.size(); ++i)
	{
		auto& passBuilder = m_internal->passBuilders[i];

		CompiledPass pass;
		pass.name      = passBuilder.m_name;
		pass.pipeline  = std::move(passBuilder.m_pipeline);
		pass.executeFn = std::move(passBuilder.m_fn);
		pass.preFn     = std::move(passBuilder.m_preFn);
		pass.postFn    = std::move(passBuilder.m_postFn);
		pass.condition = std::move(passBuilder.m_condition);

		for (const auto& acquire : acquireList[i])
			pass.acquires.push_back({ acquire, resourceMetas[acquire].combinedUsage, resourceMetas[acquire].info->format, resourceMetas[acquire].info->size });

		for (const auto& acc : passBuilder.m_accesses)
		{
			bool needsTransition = true;

			EPassType passType = EPassType::Unknown;
			if (std::holds_alternative<std::unique_ptr<ga::gpu::GraphicsPipeline>>(pass.pipeline))
				passType = EPassType::Render;
			if (std::holds_alternative<std::unique_ptr<ga::gpu::ComputePipeline>>(pass.pipeline))
				passType = EPassType::Compute;
			if (std::holds_alternative<std::unique_ptr<ga::gpu::RaytracingPipeline>>(pass.pipeline))
				passType = EPassType::Raytracing;
			if (std::holds_alternative<std::nullptr_t>(pass.pipeline))
				passType = EPassType::Transfer;

			if (currentStates.contains(acc.id))
				needsTransition = (s_UsageToLayout(acc.usage) != currentStates[acc.id].layout);

			if (needsTransition)
			{
				ga::gpu::TransitionInfo transitionInfo;

				if (currentStates.contains(acc.id))
				{
					auto& previousState = currentStates[acc.id];

					std::tie(
						transitionInfo.srcAccess,
						transitionInfo.srcStage,
						std::ignore
					) = s_GetBarrierInfo(previousState.usage, previousState.isWrite, previousState.passType);
				}
				else
				{
					transitionInfo.srcAccess = gpu::EAccessType::None;
					transitionInfo.srcStage  = gpu::EPipelineStage::TopOfPipe;
				}

				std::tie(
					transitionInfo.dstAccess,
					transitionInfo.dstStage,
					transitionInfo.dstLayout
				) = s_GetBarrierInfo(acc.usage, acc.isWrite, passType);

				pass.transitions.push_back({ acc.id, transitionInfo });
				currentStates[acc.id] = { acc.usage, transitionInfo.dstLayout, acc.isWrite, passType };
			}
		}

		for (const auto& release : releaseList[i])
			pass.releases.push_back({ release });

		for (const auto& [lhs, rhs] : passBuilder.m_swaps)
		{
			if (!m_internal->persistentImages.contains(lhs) || !m_internal->persistentImages.contains(rhs))
				throw std::runtime_error("Cannot swap non-persistent resources");

			pass.swaps.push_back({ lhs, rhs });
		}

		for (const auto& acc : passBuilder.m_accesses)
		{
			if (!acc.isWrite)
				continue;

			if ((acc.usage & gpu::EImageUsage::ColorAttachment) != gpu::EImageUsage::None)
				pass.colorAttachments.push_back(ResolvedAttachment {
					.id         = acc.id,
					.loadOp     = acc.loadOp,
					.storeOp    = acc.storeOp,
					.clearValue = acc.clearValue
				});

			else if ((acc.usage & gpu::EImageUsage::DepthStencil) != gpu::EImageUsage::None)
				pass.depthAttachment = ResolvedAttachment {
					.id         = acc.id,
					.loadOp     = acc.loadOp,
					.storeOp    = acc.storeOp,
					.clearValue = acc.clearValue
				};
		}

		m_internal->passes.emplace_back();
		m_internal->passes.back() = std::move(pass);
	}

	m_internal->passBuilders.clear();

#ifdef ENABLE_RENDERGRAPH_DUMP
	std::println("--- RenderGraph: Compilation complete");
	for (auto& p : m_internal->passes)
	{
		std::println("{}", p.name);

		std::println(" * {} acquisitions", p.acquires.size());
		for (auto& a : p.acquires)
			std::println("    > {} ({}x{}, {}, {})", a.id, a.size.x, a.size.y, uint32_t(a.usage), uint32_t(a.format));

		std::println(" * {} transitions", p.transitions.size());
		for (auto& t : p.transitions)
			std::println("    > {} (target: {})", t.id, uint32_t(t.targetUsage));

		std::println(" * {} releases", p.releases.size());
		for (auto& r : p.releases)
			std::println("    > {}", r.id);
	}
	std::println("-------------------------------------");
#endif /* ENABLE_RENDERGRAPH_DUMP */
}

void RenderGraph::Execute(const ga::gpu::CommandEncoder& encoder, const std::vector<RGBinding>& bindings)
{
	if (m_internal->passes.empty())
	{
		Compile();

		if (m_internal->passes.empty())
			return; // Render graph is empty
	}

	m_internal->activeImages.clear();
	m_internal->outputSize = { 0, 0 };

	encoder.PushLabel(std::format("RenderGraph::Execute: {}", m_name));

	for (const auto& b : bindings)
	{
		if (b.flag == ERGBindingFlags::MatchSize)
			m_internal->outputSize = b.image->Size();

		m_internal->activeImages[b.id] = b.image;
	}

	if (m_internal->outputSize == glm::uvec2{ 0, 0 })
		throw std::runtime_error("RenderGraph needs a binding with the MatchSize flag");

	for (auto& [id, pi] : m_internal->persistentImages)
	{
		glm::uvec2 size = m_internal->MatchOutputSize(pi.size);

		pi.Update(*m_internal->resourcePool, size);
		m_internal->activeImages[id] = pi.image;
	}

	for (auto& pass : m_internal->passes)
	{
		bool shouldExecute = true;
		if (pass.condition)
			shouldExecute = pass.condition();

		if (shouldExecute)
			encoder.PushLabel(pass.name);

		for (const auto& [lhs, rhs] : pass.swaps)
		{
			std::swap(m_internal->persistentImages[lhs], m_internal->persistentImages[rhs]);
			std::swap(m_internal->activeImages[lhs], m_internal->activeImages[rhs]);
		}

		for (auto& a : pass.acquires)
		{
			glm::uvec2 size = m_internal->MatchOutputSize(a.size);
			m_internal->activeImages[a.id] = m_internal->resourcePool->Acquire(a.usage, a.format, size);
		}

		for (auto& t : pass.transitions)
			encoder.Transition(*m_internal->activeImages[t.id], t.info);
		
		if (shouldExecute)
		{
			if (pass.preFn)
				pass.preFn(encoder);

			if (std::holds_alternative<std::unique_ptr<ga::gpu::GraphicsPipeline>>(pass.pipeline))
			{
				auto& pipeline = *std::get<std::unique_ptr<ga::gpu::GraphicsPipeline>>(pass.pipeline);
				auto info = s_BuildBeginRenderingInfo(pass, pipeline, m_internal->activeImages);
			
				encoder.Render(info, [&area = info.renderArea, &pass](const gpu::RenderEncoder& render) {
					render.SetViewport({ float(area.offset.x), float(area.offset.y) }, { float(area.extent.x), float(area.extent.y) });
					render.SetScissor(area.offset, area.extent);
					std::get<RenderExecuteFn>(pass.executeFn)(render);
				});
			}
			else if (std::holds_alternative<std::unique_ptr<ga::gpu::ComputePipeline>>(pass.pipeline))
			{
				ga::gpu::BeginComputeInfo info{ *std::get < std::unique_ptr<ga::gpu::ComputePipeline>>(pass.pipeline) };
				encoder.Compute(info, std::get<ComputeExecuteFn>(pass.executeFn));
			}
			else if (std::holds_alternative<std::unique_ptr<ga::gpu::RaytracingPipeline>>(pass.pipeline))
			{
				ga::gpu::BeginRayTracingInfo info { *std::get<std::unique_ptr<ga::gpu::RaytracingPipeline>>(pass.pipeline) };
				encoder.Raytrace(info, std::get<RaytraceExecuteFn>(pass.executeFn));
			}
			else if (std::holds_alternative<nullptr_t>(pass.pipeline))
			{
				encoder.Transfer(std::get<TransferExecuteFn>(pass.executeFn));
			}

			if (pass.postFn)
				pass.postFn(encoder);
		}

		for (auto& r : pass.releases)
		{
			m_internal->resourcePool->Release(m_internal->activeImages[r.id]);
			m_internal->activeImages.erase(r.id);
		}

		if (shouldExecute)
			encoder.PopLabel();
	}

	m_internal->resourcePool->Trim();
	encoder.PopLabel();
}
