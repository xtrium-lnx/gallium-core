#include "image_exr.h"

#include <stdexcept>
#include <string>
#include <print>

#define TINYEXR_USE_MINIZ 0
#include <miniminiz.h>
#define TINYEXR_IMPLEMENTATION
#include <tinyexr/tinyexr.h>

using namespace ga::assets::loaders;

ExrImageLoader::ExrImageLoader(gpu::Device& gpu)
    : m_gpu(gpu)
{}

bool ExrImageLoader::CanLoad(std::string_view ext) const
{
    return ext == ".exr";
}

std::shared_ptr<ga::gpu::Image> ExrImageLoader::Load(std::string_view path, std::span<const std::byte> bytes, AssetManager& assetManager)
{
    float* pixels = nullptr;
    int width     = 0;
    int height    = 0;

    const char* err = nullptr;

    int result = LoadEXRFromMemory(&pixels, &width, &height, reinterpret_cast<const unsigned char*>(bytes.data()), bytes.size(), &err);

    if (result != TINYEXR_SUCCESS)
    {
        std::string message = err
            ? err
            : "Unknown TinyEXR error";

        if (err)
            FreeEXRErrorMessage(err);

        std::println("ExrImageLoader: failed to decode image: {}", message);
        throw std::runtime_error("ExrImageLoader: failed to decode image: " + message);
    }

    auto exrFree = [](float* p) { free(p); };
    std::unique_ptr<float, decltype(exrFree)> pixelsHolder(pixels, exrFree);

    return std::make_shared<ga::gpu::Image>(
        m_gpu,
        ga::gpu::ImageDesc {
            .type        = gpu::EImageType::Image2D,
            .format      = gpu::EFormat::R32G32B32A32_SFloat,
            .width       = size_t(width),
            .height      = size_t(height),
            .usage       = gpu::EImageUsage::Sampled,
            .debugName   = std::string(path),
            .initialData = pixelsHolder.get(),
        }
    );
}
