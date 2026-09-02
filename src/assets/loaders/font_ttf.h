#ifndef GALLIUM__ASSETS__LOADERS__FONT_TTF_H
#define GALLIUM__ASSETS__LOADERS__FONT_TTF_H
#pragma once

#include <memory>
#include <gallium/assets/iassetloader.h>
#include <gallium/render/font.h>

namespace ga::gpu { class Device; }

namespace ga::assets::loaders
{
    class TtfFontLoader
        : public IAssetLoader<ga::render::Font>
    {
        ga::gpu::Device& m_gpu;

    public:
        explicit TtfFontLoader(ga::gpu::Device& gpu);

        bool             CanLoad(std::string_view ext) const override;
        std::shared_ptr<ga::render::Font> Load(std::string_view path, std::span<const std::byte> bytes, AssetManager& assetManager) override;
    };
}

#endif /* GALLIUM__ASSETS__LOADERS__FONT_TTF_H */