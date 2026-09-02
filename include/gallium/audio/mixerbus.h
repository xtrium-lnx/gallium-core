#ifndef GALLIUM__AUDIO__MIXERBUS_H
#define GALLIUM__AUDIO__MIXERBUS_H
#pragma once

#include <gallium/audio/audioeffect.h>

#include <array>
#include <cstdint>
#include <memory>
#include <string>

#include <glm/glm.hpp>

namespace ga::audio
{
    static constexpr uint32_t k_MaxBusEffects = 16;

    struct MixerBusHandle
    {
        uint32_t id = UINT32_MAX;
        bool IsValid() const { return id != UINT32_MAX; }
        bool operator==(const MixerBusHandle&) const = default;
    };

    class MixerBus
    {
        friend class AudioSystem; // for ma_sound_group access and solo resolution

        struct Impl;
        std::unique_ptr<Impl> m_pImpl;

    public:
        explicit MixerBus(uint32_t id, std::string name);
        ~MixerBus();

        MixerBus(MixerBus&&) noexcept;
        MixerBus& operator=(MixerBus&&) noexcept;

        MixerBus(const MixerBus&)            = delete;
        MixerBus& operator=(const MixerBus&) = delete;

        MixerBusHandle GetHandle() const;

        void               SetName(std::string name);
        const std::string& GetName() const;

        glm::vec2 GetPeak() const;

        void  SetVolume(float volume); // 0..1 nominal, >1 for gain
        float GetVolume() const;

        void  SetPan(float pan);
        float GetPan() const;

        void SetMute(bool mute);
        bool GetMute() const;
        void SetSolo(bool solo);
        bool GetSolo() const;

        void SetEffect(uint32_t slot, std::unique_ptr<IAudioEffect> effect);
        void ClearEffect(uint32_t slot);
        IAudioEffect* GetEffect(uint32_t slot) const; // may be null
    };
}

#endif /* GALLIUM__AUDIO__MIXERBUS_H */