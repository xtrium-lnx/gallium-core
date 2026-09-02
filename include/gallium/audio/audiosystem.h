#ifndef GALLIUM__AUDIO__AUDIOSYSTEM_H
#define GALLIUM__AUDIO__AUDIOSYSTEM_H
#pragma once

#include <gallium/audio/audiosource.h>
#include <gallium/audio/mixerbus.h>
#include <gallium/audio/sound.h>

#include <glm/glm.hpp>

#include <memory>
#include <span>
#include <string>
#include <string_view>

namespace ga::platform { class Platform; }

namespace ga::audio
{
    enum class ELoadHint
    {
        Auto,
        Static,
        Streamed
    };

    struct AudioSourceInfo
    {
        SoundHandle    sound;
        MixerBusHandle bus;
        float          volume = 1.0f;
        float          pitch  = 1.0f;
        bool           loop   = false;
    };

    class AudioSystem
    {
        struct Impl;
        std::unique_ptr<Impl> m_pImpl;

    public:
        explicit AudioSystem(ga::platform::Platform& vfs);
        ~AudioSystem();

        AudioSystem(const AudioSystem&)            = delete;
        AudioSystem& operator=(const AudioSystem&) = delete;

        MixerBusHandle CreateBus(std::string name);
        void           DestroyBus(MixerBusHandle handle);
        MixerBus&      GetBus(MixerBusHandle handle);
        const MixerBus& GetBus(MixerBusHandle handle) const;

        MixerBusHandle GetMasterBus() const;

        void  AddSend(MixerBusHandle from, MixerBusHandle to, float gain = 1.0f);
        void  RemoveSend(MixerBusHandle from, MixerBusHandle to);
        void  SetSendGain(MixerBusHandle from, MixerBusHandle to, float gain);
        float GetSendGain(MixerBusHandle from, MixerBusHandle to) const;

        uint32_t       GetBusCount() const;
        MixerBusHandle GetBusByIndex(uint32_t index) const;

        SoundHandle LoadSound(std::string_view path, ELoadHint hint = ELoadHint::Auto);
        void        UnloadSound(SoundHandle handle);
        bool        IsSoundLoaded(SoundHandle handle) const;

        AudioSourceHandle  CreateSource(const AudioSourceInfo& desc);
        void               DestroySource(AudioSourceHandle handle);
        bool               IsSourceValid(AudioSourceHandle handle) const;
        AudioSource&       GetSource(AudioSourceHandle handle);
        const AudioSource& GetSource(AudioSourceHandle handle) const;

        void   Play(AudioSourceHandle handle);              // no-op if already Playing
        void   Pause(AudioSourceHandle handle);             // retains position, releases voice
        void   Stop(AudioSourceHandle handle);              // resets position to 0, releases voice
        void   Seek(AudioSourceHandle handle, double seconds); // valid in any state
        double GetPlaybackPosition(AudioSourceHandle handle) const;

        void SetSourceBus(AudioSourceHandle handle, MixerBusHandle bus);

        void      SetListenerTransform(const glm::vec3& position, const glm::vec3& forward,  const glm::vec3& up);
        glm::vec3 GetListenerPosition() const;

        void   SetMasterClockSource(AudioSourceHandle handle);
        double GetMasterClock() const; // seconds

        void Update();
    };
}

#endif /* GALLIUM__AUDIO__AUDIOSYSTEM_H */