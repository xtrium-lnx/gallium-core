#ifndef GALLIUM__ASSETS__LOADERS__IMAGE_STB_H
#define GALLIUM__ASSETS__LOADERS__IMAGE_STB_H
#pragma once

#include <gallium/assets/iassetloader.h>
#include <gallium/gpu/image.h>

namespace ga::gpu { class Device; }

namespace ga::assets::loaders
{
    class StbImageLoader
        : public IAssetLoader<ga::gpu::Image>
    {
        gpu::Device& m_gpu;

    public:
        explicit StbImageLoader(gpu::Device& gpu);

        bool                            CanLoad(std::string_view ext) const override;
        std::shared_ptr<ga::gpu::Image> Load(std::string_view path, std::span<const std::byte> bytes, AssetManager& assetManager) override;
    };
}

#endif /* GALLIUM__ASSETS__LOADERS__IMAGE_STB_H */