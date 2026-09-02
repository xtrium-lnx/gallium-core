#ifndef GALLIUM__ASSETS__LOADERS__MESH_OBJ_H
#define GALLIUM__ASSETS__LOADERS__MESH_OBJ_H
#pragma once

#include <memory>
#include <gallium/assets/iassetloader.h>
#include <gallium/render/mesh.h>

namespace ga::gpu { class Device; }

namespace ga::assets::loaders
{
    class ObjMeshLoader
        : public IAssetLoader<ga::render::Mesh>
    {
        ga::gpu::Device& m_gpu;

    public:
        explicit ObjMeshLoader(ga::gpu::Device& gpu);

        bool                              CanLoad(std::string_view ext) const override;
        std::shared_ptr<ga::render::Mesh> Load(std::string_view, std::span<const std::byte> bytes, AssetManager& assetManager) override;
    };
}

#endif /* GALLIUM__ASSETS__LOADERS__MESH_OBJ_H */
