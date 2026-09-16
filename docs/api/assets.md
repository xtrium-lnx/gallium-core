# Assets API

The asset API provides typed resource loading and management.

### `include/gallium/assets/iassetloader.h`

`AssetManager`, `IAssetLoader`

### `include/gallium/assets/assetmanager.h`

`Platform`, `AudioSystem`, `AssetManager`, `AssetManagerCreateInfo`


## Typical call

```cpp
auto mesh =
    ga::assets::AssetManager::Global()
        .Load<ga::render::Mesh>("/assets/model.glb");
```

See [Assets](../assets.md) for the conceptual model.
