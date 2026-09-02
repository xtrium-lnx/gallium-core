#include <gallium/audio/audiosource.h>
#include "audiosource_impl.h"

#include <cassert>

using namespace ga::audio;

AudioSource::AudioSource(uint32_t id, SoundHandle sound, MixerBusHandle bus, float volume, float pitch, bool loop)
    : m_pImpl(std::make_unique<Impl>())
{
    m_pImpl->id = id;
    m_pImpl->sound = sound;
    m_pImpl->bus = bus;
    m_pImpl->volume = volume;
    m_pImpl->pitch = pitch;
    m_pImpl->loop = loop;
}

AudioSource::~AudioSource()
{
    assert(!m_pImpl || !m_pImpl->maSoundInitialised
        && "AudioSource destroyed with live voice - call DestroySource() first");
}

AudioSource::AudioSource(AudioSource&&) noexcept = default;
AudioSource& AudioSource::operator=(AudioSource&&) noexcept = default;

AudioSourceHandle AudioSource::GetHandle() const
{
    return { m_pImpl->id, m_pImpl->version };
}

ESourceState AudioSource::GetState() const
{
    return m_pImpl->state;
}

void AudioSource::SetVolume(float volume)
{
    m_pImpl->volume = volume;
    if (m_pImpl->maSoundInitialised)
        ma_sound_set_volume(&m_pImpl->maSound, volume);
}

float AudioSource::GetVolume() const { return m_pImpl->volume; }

void AudioSource::SetPitch(float pitch)
{
    m_pImpl->pitch = pitch;
    if (m_pImpl->maSoundInitialised)
        ma_sound_set_pitch(&m_pImpl->maSound, pitch);
}

float AudioSource::GetPitch() const { return m_pImpl->pitch; }

void AudioSource::SetLoop(bool loop)
{
    m_pImpl->loop = loop;
    if (m_pImpl->maSoundInitialised)
        ma_sound_set_looping(&m_pImpl->maSound, loop ? MA_TRUE : MA_FALSE);
}

bool AudioSource::GetLoop() const { return m_pImpl->loop; }

MixerBusHandle AudioSource::GetBus() const { return m_pImpl->bus; }

void AudioSource::SetPosition(const glm::vec3& pos)
{
    m_pImpl->position = pos;
    m_pImpl->spatial = true;
    if (m_pImpl->maSoundInitialised)
        ma_sound_set_position(&m_pImpl->maSound, pos.x, pos.y, pos.z);
}

void AudioSource::SetVelocity(const glm::vec3& vel)
{
    m_pImpl->velocity = vel;
    if (m_pImpl->maSoundInitialised)
        ma_sound_set_velocity(&m_pImpl->maSound, vel.x, vel.y, vel.z);
}

glm::vec3 AudioSource::GetPosition() const { return m_pImpl->position; }
bool      AudioSource::IsSpatial()   const { return m_pImpl->spatial; }
