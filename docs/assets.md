# Assets

The asset system turns application-facing paths into typed runtime resources.

## Asset manager

The global entry point used throughout the supplied code is:

```cpp
ga::assets::AssetManager::Global()
```

Then request a resource by type:

```cpp
auto mesh =
    ga::assets::AssetManager::Global()
        .Load<ga::render::Mesh>("/assets/model.glb");
```

Other examples include:

```cpp
auto image =
    ga::assets::AssetManager::Global()
        .Load<ga::gpu::Image>("/assets/env.hdr");
```

and sound resources used by the audio subsystem.

## Typed loading

The template argument is significant:

```cpp
Load<ga::render::Mesh>(...)
Load<ga::gpu::Image>(...)
```

It tells Gallium what kind of resource the caller expects.

This allows Calcium3D components to remain simple:

```text
Mesh component
   ↓
AssetManager
   ↓
ga::render::Mesh
```

## Asset paths

The supplied example uses project-style absolute asset paths such as:

```text
/assets/DamagedHelmet.glb
/assets/plane.glb
```

The leading `/assets/` is therefore part of the application's resource namespace, not necessarily a native filesystem path.

!!! tip "Keep paths engine-relative"
    Prefer the asset namespace used by the project rather than baking machine-specific filesystem paths into game code.

## Lifetime

Assets are managed by Gallium rather than by each component.

That means a mesh can be requested by multiple users without every component inventing its own ownership system.

The exact ownership/cache behavior should be treated according to the `AssetManager` API rather than inferred from a single caller.

## Adding a resource type

Gallium's architecture is designed around typed resource loaders. A custom loader/resource type can therefore be integrated without changing every application call site.

For a new resource, think in three pieces:

```text
resource type
   +
loader
   +
asset-manager registration
```
