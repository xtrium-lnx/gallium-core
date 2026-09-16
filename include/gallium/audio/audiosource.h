#ifndef GALLIUM__AUDIO__AUDIOSOURCE_H
#define GALLIUM__AUDIO__AUDIOSOURCE_H
#pragma once

#include <gallium/core/ctti.h>
#include <gallium/audio/mixerbus.h>
#include <gallium/audio/sound.h>

#include <glm/glm.hpp>
#include <cstdint>
#include <memory>

namespace ga::audio
{
    enum class ESourceState
    {
        Stopped,
        Playing,
        Paused,
        Finished,
    };

    struct AudioSourceHandle
    {
        uint32_t id      = UINT32_MAX;
        uint32_t version = 0;
        bool IsValid() const { return id != UINT32_MAX; }
        bool operator==(const AudioSourceHandle&) const = default;
    };

    class AudioSource
        : public ga::core::CttiObject
    {
        GA_CTTI_OBJECT(ga::audio::AudioSource, ga::core::CttiObject);

        friend class AudioSystem;

        struct Impl;
        std::unique_ptr<Impl> m_pImpl;

    public:
        AudioSource(uint32_t id, SoundHandle sound, MixerBusHandle bus, float volume, float pitch, bool loop);
        AudioSource(ga::core::CttiDeserializer& deserializer);
        ~AudioSource();

        AudioSource(AudioSource&&) noexcept;
        AudioSource& operator=(AudioSource&&) noexcept;

        AudioSource(const AudioSource&)            = delete;
        AudioSource& operator=(const AudioSource&) = delete;

        AudioSourceHandle GetHandle() const;
        ESourceState GetState() const;

        void  SetVolume(float volume);
        float GetVolume() const;

        void  SetPitch(float pitch);
        float GetPitch() const;

        void  SetLoop(bool loop);
        bool  GetLoop() const;

        MixerBusHandle GetBus() const;

        void      SetPosition(const glm::vec3& position);
        void      SetVelocity(const glm::vec3& velocity);
        glm::vec3 GetPosition() const;
        bool      IsSpatial() const;
        void      DisableSpatial();
    };
}

template<>
struct std::hash<ga::audio::AudioSourceHandle>
{
    std::size_t operator()(const ga::audio::AudioSourceHandle& s) const noexcept
    {
        return s.id;
    }
};

#endif /* GALLIUM__AUDIO__AUDIOSOURCE_H */