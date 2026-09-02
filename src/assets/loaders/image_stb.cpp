#include "image_stb.h"

#include <stdexcept>
#include <string>
#include <print>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

using namespace ga::assets::loaders;

StbImageLoader::StbImageLoader(gpu::Device& gpu)
    : m_gpu(gpu)
{}

bool StbImageLoader::CanLoad(std::string_view ext) const
{
    return ext == ".png"
        || ext == ".jpg"
        || ext == ".jpeg"
        || ext == ".tga"
        || ext == ".bmp"
        || ext == ".hdr";
}

std::shared_ptr<ga::gpu::Image> StbImageLoader::Load(std::string_view path, std::span<const std::byte> bytes, AssetManager& assetManager)
{
    int width, height, channels;

    auto stbFree = [](void* p) { stbi_image_free(p); };

    if (path.ends_with(".hdr"))
    {
        std::unique_ptr<float, decltype(stbFree)> pixels(
            stbi_loadf_from_memory(reinterpret_cast<const stbi_uc*>(bytes.data()), int(bytes.size()), &width, &height, &channels, STBI_rgb_alpha),
            stbFree
        );

        if (!pixels)
        {
            std::println("StbImageLoader: failed to decode image: {}", stbi_failure_reason());
            throw std::runtime_error(std::string("StbImageLoader: failed to decode image: ") + stbi_failure_reason());
        }

        return std::make_shared<ga::gpu::Image>(m_gpu, ga::gpu::ImageDesc {
            .type        = ga::gpu::EImageType::Image2D,
            .format      = ga::gpu::EFormat::R32G32B32A32_SFloat,
            .width       = size_t(width),
            .height      = size_t(height),
            .usage       = ga::gpu::EImageUsage::Sampled,
            .initialData = pixels.get(),
        });
    }
    else
    {
        std::unique_ptr<stbi_uc, decltype(stbFree)> pixels(
            stbi_load_from_memory(reinterpret_cast<const stbi_uc*>(bytes.data()), int(bytes.size()), &width, &height, &channels, STBI_rgb_alpha),
            stbFree
        );

        if (!pixels)
        {
            std::println("StbImageLoader: failed to decode image: {}", stbi_failure_reason());
            throw std::runtime_error(std::string("StbImageLoader: failed to decode image: ") + stbi_failure_reason());
        }

        return std::make_shared<ga::gpu::Image>(m_gpu, ga::gpu::ImageDesc {
            .type        = ga::gpu::EImageType::Image2D,
            .format      = ga::gpu::EFormat::R8G8B8A8_UNorm,
            .width       = size_t(width),
            .height      = size_t(height),
            .usage       = ga::gpu::EImageUsage::Sampled,
            .debugName   = std::string(path),
            .initialData = pixels.get()
        });
    }
}
