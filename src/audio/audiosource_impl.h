#pragma once

#include <gallium/audio/audiosource.h>
#include <miniaudio/miniaudio.h>

#include <glm/glm.hpp>

struct ga::audio::AudioSource::Impl
{
    uint32_t                  id;
    uint32_t                  version = 0;

    ga::audio::SoundHandle    sound;
    ga::audio::MixerBusHandle bus;

    ga::audio::ESourceState   state   = ga::audio::ESourceState::Stopped;
    float                     volume  = 1.0f;
    float                     pitch   = 1.0f;
    bool                      loop    = false;
    bool                      spatial = false;

    glm::vec3 position = { 0.f, 0.f, 0.f };
    glm::vec3 velocity = { 0.f, 0.f, 0.f };

    double    positionSeconds = 0.0;

    ma_sound  maSound;
    bool      maSoundInitialised = false;
};
