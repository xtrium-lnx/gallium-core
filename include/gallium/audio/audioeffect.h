#ifndef GALLIUM__AUDIO__AUDIOEFFECT_H
#define GALLIUM__AUDIO__AUDIOEFFECT_H
#pragma once

#define NOMINMAX

#include <algorithm>
#include <cstdint>
#include <cmath>

namespace ga::audio
{
    struct IAudioEffect
    {
        virtual ~IAudioEffect() = default;
        virtual void Process(float*   frames, uint32_t frameCount, uint32_t channels) = 0;
    };

    struct GainEffect
        : IAudioEffect
    {
        float gain;
        explicit GainEffect(float g)
            : gain(g)
        {}

        void Process(float* frames, uint32_t frameCount, uint32_t channels) override
        {
            const uint32_t n = frameCount * channels;
            for (uint32_t i = 0; i < n; ++i)
                frames[i] *= gain;
        }
    };

    struct LowPassEffect
        : IAudioEffect
    {
        float cutoffHz;
        float sampleRate;

        LowPassEffect(float cutoff, float sr)
            : cutoffHz(cutoff)
            , sampleRate(sr)
        {
            _update();
        }

        void SetCutoff(float hz)
        {
            cutoffHz = hz;
            _update();
        }

        void Process(float* frames, uint32_t frameCount, uint32_t channels) override
        {
            const uint32_t ch = std::min(channels, 8u);
            for (uint32_t c = 0; c < ch; ++c)
            {
                float z = _z[c];
                for (uint32_t f = 0; f < frameCount; ++f)
                {
                    float& s = frames[f * channels + c];
                    z = z + _alpha * (s - z);
                    s = z;
                }
                _z[c] = z;
            }
        }

    private:
        float _alpha = 1.0f;
        float _z[8]  = {};
        void _update()
        {
            const float rc = 1.0f / (2.0f * 3.14159265f * cutoffHz);
            const float dt = 1.0f / sampleRate;
            _alpha = dt / (rc + dt);
        }
    };

    struct HighPassEffect
        : IAudioEffect
    {
        float cutoffHz;
        float sampleRate;

        HighPassEffect(float cutoff, float sr)
            : cutoffHz(cutoff)
            , sampleRate(sr)
        {
            _update();
        }

        void SetCutoff(float hz)
        {
            cutoffHz = hz;
            _update();
        }

        void Process(float* frames, uint32_t frameCount, uint32_t channels) override
        {
            const uint32_t ch = std::min(channels, 8u);
            for (uint32_t c = 0; c < ch; ++c)
            {
                float pi = _prevIn[c], po = _prevOut[c];
                for (uint32_t f = 0; f < frameCount; ++f)
                {
                    float& s = frames[f * channels + c];
                    float out = _alpha * (po + s - pi);
                    pi = s; po = out; s = out;
                }
                _prevIn[c] = pi; _prevOut[c] = po;
            }
        }

    private:
        float _alpha    = 1.0f;
        float _prevIn[8]  = {};
        float _prevOut[8] = {};
        void _update()
        {
            const float rc = 1.0f / (2.0f * 3.14159265f * cutoffHz);
            const float dt = 1.0f / sampleRate;
            _alpha = rc / (rc + dt);
        }
    };
}

#endif /* GALLIUM__AUDIO__AUDIOEFFECT_H */