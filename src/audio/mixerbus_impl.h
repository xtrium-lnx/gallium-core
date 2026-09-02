#pragma once

#include <gallium/audio/mixerbus.h>
#include <miniaudio/miniaudio.h>

#include <array>
#include <memory>
#include <string>


struct PeakNode
{
    ma_node_base       base;
    uint32_t           channelCount = 0;
    std::atomic<float> lPeak { 0.f };
    std::atomic<float> rPeak { 0.f };
};

struct ga::audio::MixerBus::Impl
{
    uint32_t       id;
    std::string    name;
    float          volume = 1.0f;
    float          pan    = 0.0f;
    bool           mute   = false;
    bool           solo   = false;

    PeakNode       peakNode;
    glm::vec2      lastPeak = { 0.0f, 0.0f };

    ma_sound_group group;
    bool           groupInitialised = false;

    std::array<std::unique_ptr<ga::audio::IAudioEffect>, ga::audio::k_MaxBusEffects> effects;

    static ma_result s_PeakNodeInit(ma_engine* engine, uint32_t channels, PeakNode* node);
    static void s_PeakNodeUninit(PeakNode* node);
};