#include <gallium/audio/mixerbus.h>
#include "mixerbus_impl.h"

#include <cassert>
#include <string>

using namespace ga::audio;

// ----------------------------------------------------------------------------

static void s_PeakNodeProcess(ma_node* pNode, const float** ppIn, ma_uint32* pInFrameCount, float** ppOut, ma_uint32* pOutFrameCount)
{
    auto* node = reinterpret_cast<PeakNode*>(pNode);
    const float* in = ppIn[0];
    float* out = ppOut[0];
    const uint32_t frames = *pOutFrameCount;
    const uint32_t channels = node->channelCount;
    const uint32_t samples = frames * channels;

    float lPeak = 0.f;
    float rPeak = 0.f;

    for (uint32_t i = 0; i < samples; i += node->channelCount)
    {
        for (uint32_t s = 0; s < node->channelCount; ++s)
        {
            out[i + s] = in[i + s];

            if (s == 0)
                lPeak = std::max(lPeak, std::abs(in[i + s]));
            else if (s == 1)
                rPeak = std::max(rPeak, std::abs(in[i + s]));
        }
    }

    float lPrev = node->lPeak.load(std::memory_order_relaxed);
    while (lPrev < lPeak && !node->lPeak.compare_exchange_weak(lPrev, lPeak, std::memory_order_relaxed)) {}

    float rPrev = node->rPeak.load(std::memory_order_relaxed);
    while (rPrev < rPeak && !node->rPeak.compare_exchange_weak(rPrev, rPeak, std::memory_order_relaxed)) {}
}

static auto s_PeakNodeVTable = ma_node_vtable {
    .onProcess      = s_PeakNodeProcess,
    .inputBusCount  = 1,
    .outputBusCount = 1,
    .flags          = MA_NODE_FLAG_PASSTHROUGH
};

ma_result MixerBus::Impl::s_PeakNodeInit(ma_engine * engine, uint32_t channels, PeakNode * node)
{
    node->channelCount = channels;
    node->lPeak.store(0.f);
    node->rPeak.store(0.f);

    ma_node_config cfg  = ma_node_config_init();
    cfg.vtable          = &s_PeakNodeVTable;
    cfg.pInputChannels  = &channels;
    cfg.pOutputChannels = &channels;

    return ma_node_init(ma_engine_get_node_graph(engine), &cfg, nullptr, &node->base);
}

void MixerBus::Impl::s_PeakNodeUninit(PeakNode* node)
{
    ma_node_uninit(&node->base, nullptr);
}

// ----------------------------------------------------------------------------

MixerBus::MixerBus(uint32_t id, std::string name)
    : m_pImpl(std::make_unique<Impl>())
{
    m_pImpl->id = id;
    m_pImpl->name = std::move(name);
}

MixerBus::~MixerBus()
{
    if (m_pImpl && m_pImpl->groupInitialised)
        ma_sound_group_uninit(&m_pImpl->group);
}

MixerBus::MixerBus(MixerBus&&) noexcept = default;
MixerBus& MixerBus::operator=(MixerBus&&) noexcept = default;

MixerBusHandle MixerBus::GetHandle() const
{
    return MixerBusHandle{ m_pImpl->id };
}

void MixerBus::SetName(std::string name)
{
    m_pImpl->name = std::move(name);
}

const std::string& MixerBus::GetName() const
{
    return m_pImpl->name;
}

glm::vec2 MixerBus::GetPeak() const
{
    return m_pImpl->lastPeak;
}

void MixerBus::SetVolume(float volume)
{
    m_pImpl->volume = volume;
    // Actual ma_sound_group volume is applied by AudioSystem::Update()
    // after solo resolution — do not set directly here.
}

float MixerBus::GetVolume() const
{
    return m_pImpl->volume;
}

void MixerBus::SetPan(float pan)
{
    if (pan != m_pImpl->pan)
    {
        m_pImpl->pan = pan;
        ma_sound_group_set_pan(&m_pImpl->group, pan);
    }
}

float MixerBus::GetPan() const
{
    return m_pImpl->pan;
}

void MixerBus::SetMute(bool mute)
{
    m_pImpl->mute = mute;
    // AudioSystem::Update() will call ResolveSolo() this frame
}

bool MixerBus::GetMute() const
{
    return m_pImpl->mute;
}

void MixerBus::SetSolo(bool solo)
{
    m_pImpl->solo = solo;
}

bool MixerBus::GetSolo() const
{
    return m_pImpl->solo;
}

void MixerBus::SetEffect(uint32_t slot, std::unique_ptr<IAudioEffect> effect)
{
    assert(slot < k_MaxBusEffects);
    m_pImpl->effects[slot] = std::move(effect);
}

void MixerBus::ClearEffect(uint32_t slot)
{
    assert(slot < k_MaxBusEffects);
    m_pImpl->effects[slot].reset();
}

IAudioEffect* MixerBus::GetEffect(uint32_t slot) const
{
    assert(slot < k_MaxBusEffects);
    return m_pImpl->effects[slot].get();
}