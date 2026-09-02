#define MINIAUDIO_IMPLEMENTATION
#define STB_VORBIS_IMPLEMENTATION
#include <miniaudio/stb_vorbis.h>
#include <miniaudio/miniaudio.h>

#include <gallium/audio/audiosystem.h>
#include <gallium/platform/platform.h>
#include <gallium/platform/vfs.h>

#include "mixerbus_impl.h"
#include "audiosource_impl.h"

#include <algorithm>
#include <cassert>
#include <deque>
#include <memory>
#include <print>
#include <string>
#include <string_view>
#include <vector>

#include <glm/glm.hpp>

using namespace ga::audio;

static constexpr uint32_t k_MaxVoices = 32;
static constexpr uint32_t k_InvalidId = UINT32_MAX;

struct GalliumMaVfs
{
    ma_vfs_callbacks   callbacks;
    ga::platform::Vfs* vfs;
};

static ma_result s_VfsOpen(ma_vfs* pVfs, const char* pFilePath,
    ma_uint32 openMode, ma_vfs_file* pFile)
{
    if (openMode != MA_OPEN_MODE_READ)
        return MA_ERROR;

    auto* vfs = reinterpret_cast<GalliumMaVfs*>(pVfs)->vfs;
    auto result = vfs->Open(pFilePath);
    
    if (!result)
        return MA_DOES_NOT_EXIST;

    *pFile = new ga::platform::VfsFile(std::move(*result));
    return MA_SUCCESS;
}

static ma_result s_VfsOpenW(ma_vfs*, const wchar_t*, ma_uint32, ma_vfs_file*)
{
    return MA_NOT_IMPLEMENTED;
}

static ma_result s_VfsClose(ma_vfs*, ma_vfs_file file)
{
    delete static_cast<ga::platform::VfsFile*>(file);
    return MA_SUCCESS;
}

static ma_result s_VfsRead(ma_vfs*, ma_vfs_file file,
    void* pDst, size_t sizeInBytes, size_t* pBytesRead)
{
    *pBytesRead = static_cast<ga::platform::VfsFile*>(file)->Read(pDst, sizeInBytes);
    return MA_SUCCESS;
}

static ma_result s_VfsWrite(ma_vfs*, ma_vfs_file, const void*, size_t, size_t*)
{
    return MA_NOT_IMPLEMENTED;
}

static ma_result s_VfsSeek(ma_vfs*, ma_vfs_file file,
    ma_int64 offset, ma_seek_origin origin)
{
    auto* f = static_cast<ga::platform::VfsFile*>(file);
    size_t target = 0;
    switch (origin)
    {
    case ma_seek_origin_start:   target = offset;                       break;
    case ma_seek_origin_current: target = ma_int64(f->Tell()) + offset; break;
    case ma_seek_origin_end:     target = ma_int64(f->Size()) + offset; break;
    default: return MA_ERROR;
    }

    target = std::clamp(target, 0Ui64, f->Size() - 1);

    f->Seek(target);
    return MA_SUCCESS;
}

static ma_result s_VfsTell(ma_vfs*, ma_vfs_file file, ma_int64* pCursor)
{
    *pCursor = ma_int64(static_cast<ga::platform::VfsFile*>(file)->Tell());
    return MA_SUCCESS;
}

static ma_result s_VfsInfo(ma_vfs*, ma_vfs_file file, ma_file_info* pInfo)
{
    pInfo->sizeInBytes = static_cast<ga::platform::VfsFile*>(file)->Size();
    return MA_SUCCESS;
}

static ma_vfs_callbacks s_MakeVfsCallbacks()
{
    ma_vfs_callbacks cb{};
    cb.onOpen  = s_VfsOpen;
    cb.onOpenW = s_VfsOpenW;
    cb.onClose = s_VfsClose;
    cb.onRead  = s_VfsRead;
    cb.onWrite = s_VfsWrite;
    cb.onSeek  = s_VfsSeek;
    cb.onTell  = s_VfsTell;
    cb.onInfo  = s_VfsInfo;
    return cb;
}

struct SoundAsset
{
    uint32_t    id = k_InvalidId;
    bool        active = false;
    bool        streamed = false;
    std::string path;               // VFS path — retained for streamed re-open

    ma_audio_buffer buffer;
    bool            bufferInitialised = false;
};

struct Send
{
    uint32_t targetBusId = k_InvalidId;
    float    gain = 1.0f;
};

struct AudioSystem::Impl
{
    ma_engine               engine;
    bool                    engineInitialised = false;

    GalliumMaVfs            maVfs;

    std::deque<MixerBus>    buses;
    std::deque<AudioSource> sources;
    std::deque<SoundAsset>  sounds;

    uint32_t                nextBusId    = 0;
    uint32_t                nextSourceId = 0;
    uint32_t                nextSoundId  = 0;
    uint32_t                activeVoices = 0;

    MixerBusHandle          masterBus;
    AudioSourceHandle       masterClockSource;

    std::vector<std::pair<uint32_t, std::vector<Send>>> sendTable;

    glm::vec3 listenerPos = { 0.f, 0.f,  0.f };
    glm::vec3 listenerForward = { 0.f, 0.f, -1.f };
    glm::vec3 listenerUp = { 0.f, 1.f,  0.f };

    explicit Impl(ga::platform::Vfs& vfs)
    {
        maVfs.callbacks = s_MakeVfsCallbacks();
        maVfs.vfs       = &vfs;
    }

    MixerBus* FindBus(MixerBusHandle h)
    {
        if (!h.IsValid()) return nullptr;
        for (auto& b : buses) if (b.m_pImpl->id == h.id) return &b;
        return nullptr;
    }
    const MixerBus* FindBus(MixerBusHandle h) const
    {
        if (!h.IsValid()) return nullptr;
        for (const auto& b : buses) if (b.m_pImpl->id == h.id) return &b;
        return nullptr;
    }
    AudioSource* FindSource(AudioSourceHandle h)
    {
        if (!h.IsValid()) return nullptr;
        for (auto& s : sources)
            if (s.m_pImpl->id == h.id && s.m_pImpl->version == h.version) return &s;
        return nullptr;
    }
    const AudioSource* FindSource(AudioSourceHandle h) const
    {
        if (!h.IsValid()) return nullptr;
        for (const auto& s : sources)
            if (s.m_pImpl->id == h.id && s.m_pImpl->version == h.version) return &s;
        return nullptr;
    }
    SoundAsset* FindSound(SoundHandle h)
    {
        if (!h.IsValid()) return nullptr;
        for (auto& s : sounds) if (s.active && s.id == h.id) return &s;
        return nullptr;
    }
    std::vector<Send>* FindSends(uint32_t busId)
    {
        for (auto& [id, sends] : sendTable) if (id == busId) return &sends;
        return nullptr;
    }

    bool AcquireVoice(AudioSource& src)
    {
        if (activeVoices >= k_MaxVoices)
        {
            std::println("AudioSystem: voice limit ({}) reached", k_MaxVoices);
            return false;
        }

        auto& si = *src.m_pImpl;
        SoundAsset* asset = FindSound(si.sound);
        if (!asset) { std::println("AudioSystem: sound not found"); return false; }

        MixerBus* bus = FindBus(si.bus);
        if (!bus)   bus = FindBus(masterBus);
        if (!bus) { std::println("AudioSystem: no bus available"); return false; }

        ma_sound_config cfg = ma_sound_config_init();
        cfg.pInitialAttachment = &bus->m_pImpl->group;

        if (asset->streamed)
        {
            cfg.pFilePath = asset->path.c_str();
            cfg.flags     = MA_SOUND_FLAG_STREAM;
        }
        else
        {
            cfg.pDataSource = &asset->buffer;
        }

        if (auto result = ma_sound_init_ex(&engine, &cfg, &si.maSound); result != MA_SUCCESS)
        {
            std::println("AudioSystem: ma_sound_init_ex failed for '{}'", asset->path);
            return false;
        }

        si.maSoundInitialised = true;
        ++activeVoices;

        ma_sound_set_volume(&si.maSound, si.volume);
        ma_sound_set_pitch(&si.maSound, si.pitch);
        ma_sound_set_looping(&si.maSound, si.loop ? MA_TRUE : MA_FALSE);
        ma_sound_set_spatialization_enabled(&si.maSound, si.spatial ? MA_TRUE : MA_FALSE);
        if (si.spatial)
        {
            ma_sound_set_position(&si.maSound, si.position.x, si.position.y, si.position.z);
            ma_sound_set_velocity(&si.maSound, si.velocity.x, si.velocity.y, si.velocity.z);
        }
        if (si.positionSeconds > 0.0)
        {
            const uint64_t frame = uint64_t(si.positionSeconds * ma_engine_get_sample_rate(&engine));
            ma_sound_seek_to_pcm_frame(&si.maSound, frame);
        }

        return true;
    }

    void ReleaseVoice(AudioSource& src)
    {
        auto& si = *src.m_pImpl;
        if (!si.maSoundInitialised) return;

        uint64_t frame = 0;
        ma_sound_get_cursor_in_pcm_frames(&si.maSound, &frame);
        const uint32_t sr = ma_engine_get_sample_rate(&engine);
        si.positionSeconds = sr > 0 ? double(frame) / double(sr) : 0.0;

        ma_sound_uninit(&si.maSound);
        si.maSoundInitialised = false;
        --activeVoices;
    }

    void ResolveSolo()
    {
        bool anySolo = false;
        for (auto& b : buses) if (b.m_pImpl->solo) { anySolo = true; break; }
        for (auto& b : buses)
        {
            if (!b.m_pImpl->groupInitialised) continue;
            const bool audible = !b.m_pImpl->mute && (!anySolo || b.m_pImpl->solo);
            ma_sound_group_set_volume(&b.m_pImpl->group, audible ? b.m_pImpl->volume : 0.0f);
        }
    }
};

AudioSystem::AudioSystem(ga::platform::Platform& platform)
    : m_pImpl(std::make_unique<Impl>(platform.Vfs()))
{
    auto& impl = *m_pImpl;

    ma_engine_config cfg = ma_engine_config_init();
    cfg.listenerCount = 1;
    cfg.pResourceManagerVFS = &impl.maVfs;

    if (ma_engine_init(&cfg, &impl.engine) != MA_SUCCESS)
        throw std::runtime_error("AudioSystem: failed to initialise ma_engine");

    impl.engineInitialised = true;
    std::println("AudioSystem: ready ({}Hz, {} ch)",
        ma_engine_get_sample_rate(&impl.engine),
        ma_engine_get_channels(&impl.engine));

    m_pImpl->masterBus = CreateBus("MASTER");
}

AudioSystem::~AudioSystem()
{
    if (!m_pImpl) return;
    auto& impl = *m_pImpl;

    while (!impl.sources.empty())
        DestroySource(impl.sources.front().GetHandle());

    for (auto& asset : impl.sounds)
        if (asset.bufferInitialised)
            ma_audio_buffer_uninit(&asset.buffer);
    
    while (!impl.buses.empty())
        DestroyBus(impl.buses.front().GetHandle());

    if (impl.engineInitialised)
        ma_engine_uninit(&impl.engine);
}

MixerBusHandle AudioSystem::CreateBus(std::string name)
{
    auto& impl = *m_pImpl;
    const uint32_t id = impl.nextBusId++;

    impl.buses.emplace_back(id, name);
    MixerBus& bus = impl.buses.back();

    if (ma_sound_group_init(&impl.engine, 0, nullptr, &bus.m_pImpl->group) != MA_SUCCESS)
    {
        impl.buses.pop_back();
        std::println("AudioSystem: failed to create bus '{}'", name);
        return {};
    }
    bus.m_pImpl->groupInitialised = true;

    uint32_t ch = ma_engine_get_channels(&impl.engine);
    if (MixerBus::Impl::s_PeakNodeInit(&impl.engine, ch, &bus.m_pImpl->peakNode) == MA_SUCCESS)
    {
        ma_node_detach_output_bus(&bus.m_pImpl->group, 0);
        ma_node_attach_output_bus(&bus.m_pImpl->group, 0, &bus.m_pImpl->peakNode, 0);
        ma_node_attach_output_bus(&bus.m_pImpl->peakNode, 0, ma_engine_get_endpoint(&impl.engine), 0);
    }

    impl.sendTable.push_back({ id, {} });
    std::println("AudioSystem: bus [{}] '{}' created", id, name);
    return MixerBusHandle{ id };
}

void AudioSystem::DestroyBus(MixerBusHandle handle)
{
    auto& impl = *m_pImpl;
    if (!impl.FindBus(handle))
        return;

    for (auto& src : impl.sources)
        if (src.m_pImpl->bus == handle)
            src.m_pImpl->bus = impl.masterBus;

    auto sit = std::find_if(impl.sendTable.begin(), impl.sendTable.end(), [&](const auto& p) { return p.first == handle.id; });
    if (sit != impl.sendTable.end())
        impl.sendTable.erase(sit);

    auto it = std::find_if(impl.buses.begin(), impl.buses.end(), [&](const MixerBus& b) { return b.m_pImpl->id == handle.id; });
    if (it != impl.buses.end())
    {
        MixerBus::Impl::s_PeakNodeUninit(&it->m_pImpl->peakNode);
        impl.buses.erase(it);
    }
}

MixerBus& AudioSystem::GetBus(MixerBusHandle handle)
{
    MixerBus* b = m_pImpl->FindBus(handle);
    assert(b && "Invalid MixerBusHandle");
    return *b;
}
const MixerBus& AudioSystem::GetBus(MixerBusHandle handle) const
{
    const MixerBus* b = m_pImpl->FindBus(handle);
    assert(b && "Invalid MixerBusHandle");
    return *b;
}

MixerBusHandle AudioSystem::GetMasterBus() const
{
    return m_pImpl->masterBus;
}

void AudioSystem::AddSend(MixerBusHandle from, MixerBusHandle to, float gain)
{
    auto& impl = *m_pImpl;
    MixerBus* f = impl.FindBus(from);
    MixerBus* t = impl.FindBus(to);
    if (!f || !t)
        return;

    auto* sends = impl.FindSends(from.id);
    if (!sends)
        return;

    for (const auto& s : *sends)
        if (s.targetBusId == to.id)
            return;

    sends->push_back({ to.id, gain });

    ma_node* out = (ma_node*)&f->m_pImpl->peakNode;
    ma_node_attach_output_bus(out, 0, &t->m_pImpl->group, 0);
}

void AudioSystem::RemoveSend(MixerBusHandle from, MixerBusHandle to)
{
    auto& impl = *m_pImpl;
    MixerBus* f = impl.FindBus(from);
    if (!f)
        return;

    auto* sends = impl.FindSends(from.id);
    if (!sends)
        return;

    auto it = std::find_if(sends->begin(), sends->end(), [&](const Send& s) { return s.targetBusId == to.id; });
    if (it == sends->end())
        return;

    sends->erase(it);

    ma_node* outNode = (ma_node*)&f->m_pImpl->peakNode;
    ma_node_detach_output_bus(outNode, 0);

    if (!sends->empty())
    {
        MixerBus* remaining = impl.FindBus(MixerBusHandle { sends->front().targetBusId });
        if (remaining)
        {
            ma_node_attach_output_bus(outNode, 0, &remaining->m_pImpl->group, 0);
            ma_node_set_output_bus_volume(outNode, 0, sends->front().gain);
        }
    }
    else
        ma_node_attach_output_bus(outNode, 0, ma_engine_get_endpoint(&impl.engine), 0);
}

void AudioSystem::SetSendGain(MixerBusHandle from, MixerBusHandle to, float gain)
{
    if (auto* sends = m_pImpl->FindSends(from.id))
        for (auto& s : *sends)
            if (s.targetBusId == to.id)
            {
                s.gain = gain;
                break;
            }

    if (MixerBus* f = m_pImpl->FindBus(from))
    {
        ma_node* outNode = (ma_node*)&f->m_pImpl->peakNode;
        ma_node_set_output_bus_volume(outNode, 0, gain);
    }
}

float AudioSystem::GetSendGain(MixerBusHandle from, MixerBusHandle to) const
{
    if (const auto* sends = m_pImpl->FindSends(from.id))
        for (const auto& s : *sends)
            if (s.targetBusId == to.id)
                return s.gain;

    return 0.0f;
}

uint32_t AudioSystem::GetBusCount() const
{
    return uint32_t(m_pImpl->buses.size());
}

MixerBusHandle AudioSystem::GetBusByIndex(uint32_t i) const
{
    return i < m_pImpl->buses.size() ? m_pImpl->buses[i].GetHandle() : MixerBusHandle{};
}

SoundHandle AudioSystem::LoadSound(std::string_view path, ELoadHint hint)
{
    auto& impl = *m_pImpl;

    auto it = std::find_if(m_pImpl->sounds.begin(), m_pImpl->sounds.end(), [key = std::string(path)](const SoundAsset& s) {
        return s.path == key;
    });

    if (it != m_pImpl->sounds.end())
        return SoundHandle { it->id };

    bool streamed = false;

    // xtrium 2026-05-02: All sounds are now non-streamed to remove seek delay
    //if (hint == ELoadHint::Streamed)
    //    streamed = true;
    //else if (hint == ELoadHint::Static)
    //    streamed = false;
    //else
    //{
    //    const auto dot = path.rfind('.');
    //    const auto ext = dot != std::string_view::npos ? path.substr(dot) : "";
    //    streamed = (ext == ".mp3" || ext == ".ogg" || ext == ".flac");
    //}

    impl.sounds.push_back({});
    SoundAsset& asset = impl.sounds.back();
    asset.id = impl.nextSoundId++;
    asset.active = true;
    asset.streamed = streamed;
    asset.path = std::string(path);

    // xtrium 2026-05-02: All sounds are now non-streamed to remove seek delay
    //if (!streamed)
    //{
    //    auto fileResult = impl.maVfs.vfs->Open(path);
    //    if (!fileResult)
    //    {
    //        std::println("AudioSystem: VFS cannot open '{}'", path);
    //        asset.active = false;
    //        return {};
    //    }
    //    auto bytesResult = fileResult->ReadBytes();
    //    if (!bytesResult)
    //    {
    //        std::println("AudioSystem: failed to read '{}'", path);
    //        asset.active = false;
    //        return {};
    //    }

    //    const auto& bytes = *bytesResult;
    //    ma_decoder decoder;
    //    ma_decoder_config dcfg = ma_decoder_config_init(ma_format_f32, 0, 0);
    //    if (ma_decoder_init_memory(bytes.data(), bytes.size(), &dcfg, &decoder) != MA_SUCCESS)
    //    {
    //        std::println("AudioSystem: failed to decode '{}'", path);
    //        asset.active = false;
    //        return {};
    //    }

    //    ma_uint64 frameCount = 0;
    //    ma_decoder_get_length_in_pcm_frames(&decoder, &frameCount);
    //    const uint32_t ch = decoder.outputChannels;
    //    const uint32_t sr = decoder.outputSampleRate;

    //    std::vector<float> pcm(frameCount * ch);
    //    ma_decoder_read_pcm_frames(&decoder, pcm.data(), frameCount, nullptr);
    //    ma_decoder_uninit(&decoder);

    //    ma_audio_buffer_config bcfg = ma_audio_buffer_config_init(ma_format_f32, ch, frameCount, pcm.data(), nullptr);
    //    bcfg.sampleRate = sr;

    //    if (ma_audio_buffer_init(&bcfg, &asset.buffer) != MA_SUCCESS)
    //    {
    //        std::println("AudioSystem: failed to create audio buffer for '{}'", path);
    //        asset.active = false;
    //        return {};
    //    }
    //    asset.bufferInitialised = true;
    //    std::println("AudioSystem: loaded '{}' ({} frames, static)", path, frameCount);
    //}
    //else
    {
        auto fileResult = impl.maVfs.vfs->Open(path);
        if (!fileResult)
        {
            std::println("AudioSystem: VFS cannot open '{}'", path);
            asset.active = false;
            return {};
        }
        auto bytesResult = fileResult->ReadBytes();
        if (!bytesResult)
        {
            std::println("AudioSystem: failed to read '{}'", path);
            asset.active = false;
            return {};
        }

        const auto& bytes = *bytesResult;

        ma_uint32 engineSampleRate = ma_engine_get_sample_rate(&m_pImpl->engine);

        ma_decoder decoder;
        ma_decoder_config decoderCfg = ma_decoder_config_init(ma_format_f32, 2, engineSampleRate);
        if (ma_decoder_init_memory(bytes.data(), bytes.size(), &decoderCfg, &decoder) != MA_SUCCESS)
        {
            std::println("AudioSystem: failed to decode '{}'", path);
            asset.active = false;
            return {};
        }

        ma_uint64 totalFrames;
        ma_data_source_get_length_in_pcm_frames(&decoder, &totalFrames);

        float* pPCM = new float[totalFrames * 2]; 
        ma_decoder_read_pcm_frames(&decoder, pPCM, totalFrames, NULL);
        ma_decoder_uninit(&decoder);

        // Wrap in an audio_buffer for instant random access
        ma_audio_buffer_config bufCfg = ma_audio_buffer_config_init( ma_format_f32, 2, totalFrames, pPCM, NULL);
        ma_audio_buffer_init(&bufCfg, &asset.buffer);
        asset.bufferInitialised = true;
    }

    return SoundHandle{ asset.id };
}

void AudioSystem::UnloadSound(SoundHandle handle)
{
    SoundAsset* asset = m_pImpl->FindSound(handle);
    if (!asset)
        return;

    if (asset->bufferInitialised)
        ma_audio_buffer_uninit(&asset->buffer);

    asset->active = false;
}

bool AudioSystem::IsSoundLoaded(SoundHandle handle) const
{
    return m_pImpl->FindSound(handle) != nullptr;
}

AudioSourceHandle AudioSystem::CreateSource(const AudioSourceDesc& desc)
{
    auto& impl = *m_pImpl;
    const auto bus = desc.bus.IsValid() ? desc.bus : impl.masterBus;
    impl.sources.emplace_back(impl.nextSourceId++, desc.sound, bus, desc.volume, desc.pitch, desc.loop);
    return impl.sources.back().GetHandle();
}

void AudioSystem::DestroySource(AudioSourceHandle handle)
{
    auto& impl = *m_pImpl;
    AudioSource* src = impl.FindSource(handle);
    if (!src)
        return;

    if (src->m_pImpl->maSoundInitialised)
        impl.ReleaseVoice(*src);

    ++src->m_pImpl->version;
    impl.sources.erase(std::find_if(impl.sources.begin(), impl.sources.end(), [&](const AudioSource& s) { return s.m_pImpl->id == handle.id; }));
}

bool AudioSystem::IsSourceValid(AudioSourceHandle h) const
{
    return m_pImpl->FindSource(h) != nullptr;
}

AudioSource& AudioSystem::GetSource(AudioSourceHandle h)
{
    auto* s = m_pImpl->FindSource(h);
    assert(s);
    return *s;
}

const AudioSource& AudioSystem::GetSource(AudioSourceHandle h) const
{
    const auto* s = m_pImpl->FindSource(h);
    assert(s);
    return *s;
}

void AudioSystem::Play(AudioSourceHandle handle)
{
    AudioSource* src = m_pImpl->FindSource(handle);
    if (!src)
        return;

    auto& si = *src->m_pImpl;
    if (si.state == ESourceState::Playing)
        return;

    if (!si.maSoundInitialised && !m_pImpl->AcquireVoice(*src))
        return;

    ma_sound_start(&si.maSound);
    si.state = ESourceState::Playing;
}

void AudioSystem::Pause(AudioSourceHandle handle)
{
    AudioSource* src = m_pImpl->FindSource(handle);
    if (!src || src->m_pImpl->state != ESourceState::Playing)
        return;

    ma_sound_stop(&src->m_pImpl->maSound);
    m_pImpl->ReleaseVoice(*src);
    src->m_pImpl->state = ESourceState::Paused;
}

void AudioSystem::Stop(AudioSourceHandle handle)
{
    AudioSource* src = m_pImpl->FindSource(handle);
    if (!src || src->m_pImpl->state == ESourceState::Stopped)
        return;

    auto& si = *src->m_pImpl;
    if (si.maSoundInitialised)
    {
        ma_sound_stop(&si.maSound);
        m_pImpl->ReleaseVoice(*src);
    }

    si.positionSeconds = 0.0;
    si.state = ESourceState::Stopped;
}

void AudioSystem::Seek(AudioSourceHandle handle, double seconds)
{
    AudioSource* src = m_pImpl->FindSource(handle);
    if (!src)
        return;

    auto& si = *src->m_pImpl;
    si.positionSeconds = seconds;
    if (si.maSoundInitialised)
    {
        const uint64_t frame = uint64_t(seconds * ma_engine_get_sample_rate(&m_pImpl->engine));
        ma_sound_seek_to_pcm_frame(&si.maSound, frame);
    }
}

double AudioSystem::GetPlaybackPosition(AudioSourceHandle handle) const
{
    const AudioSource* src = m_pImpl->FindSource(handle);
    if (!src)
        return 0.0;

    const auto& si = *src->m_pImpl;
    if (si.maSoundInitialised)
    {
        uint64_t frame = 0;
        ma_sound_get_cursor_in_pcm_frames(&si.maSound, &frame);
        const uint32_t sr = ma_engine_get_sample_rate(&m_pImpl->engine);
        return sr > 0 ? double(frame) / double(sr) : 0.0;
    }
    return si.positionSeconds;
}

void AudioSystem::SetSourceBus(AudioSourceHandle handle, MixerBusHandle bus)
{
    AudioSource* src = m_pImpl->FindSource(handle);
    if (!src)
        return;

    auto& si = *src->m_pImpl;
    const bool wasPlaying = si.state == ESourceState::Playing;
    if (si.maSoundInitialised)
        m_pImpl->ReleaseVoice(*src);

    si.bus = bus;
    if (wasPlaying)
    {
        if (m_pImpl->AcquireVoice(*src))
            ma_sound_start(&si.maSound);
        else
            si.state = ESourceState::Stopped;
    }
}

void AudioSystem::SetListenerTransform(const glm::vec3& pos, const glm::vec3& forward, const glm::vec3& up)
{
    auto& impl = *m_pImpl;
    impl.listenerPos = pos; impl.listenerForward = forward; impl.listenerUp = up;
    ma_engine_listener_set_position(&impl.engine, 0, pos.x, pos.y, pos.z);
    ma_engine_listener_set_direction(&impl.engine, 0, forward.x, forward.y, forward.z);
    ma_engine_listener_set_world_up(&impl.engine, 0, up.x, up.y, up.z);
}

glm::vec3 AudioSystem::GetListenerPosition() const
{
    return m_pImpl->listenerPos;
}

void AudioSystem::SetMasterClockSource(AudioSourceHandle h)
{
    m_pImpl->masterClockSource = h;
}

double AudioSystem::GetMasterClock() const
{
    if (m_pImpl->masterClockSource.IsValid())
        return GetPlaybackPosition(m_pImpl->masterClockSource);
    return double(ma_engine_get_time_in_milliseconds(&m_pImpl->engine)) / 1000.0;
}

void AudioSystem::Update()
{
    auto& impl = *m_pImpl;
    for (auto& src : impl.sources)
    {
        auto& si = *src.m_pImpl;
        if (si.state != ESourceState::Playing || !si.maSoundInitialised || si.loop) continue;
        if (ma_sound_at_end(&si.maSound))
        {
            impl.ReleaseVoice(src);
            si.positionSeconds = 0.0;
            si.state = ESourceState::Finished;
        }
    }

    for (auto& bus : impl.buses)
    {
        auto& bi = *bus.m_pImpl;
        bi.lastPeak = glm::vec2 {
            bi.peakNode.lPeak.exchange(0.f, std::memory_order_relaxed),
            bi.peakNode.rPeak.exchange(0.f, std::memory_order_relaxed)
        };
    }

    impl.ResolveSolo();
}
