# Calcium3D integration

Gallium and Calcium3D are separate projects with a deliberate dependency boundary.

```text
Calcium3D
   ↓
Gallium
```

Calcium3D uses Gallium for its lower-level services.

## What Calcium3D consumes

The current Calcium3D code uses Gallium for:

- `ga::platform::Platform`
- `ga::gpu::Device`
- `ga::assets::AssetManager`
- `ga::audio::AudioSystem`
- renderer interfaces
- GPU images/meshes
- input
- properties
- messaging

## `GameSubsystems`

Calcium3D bundles the most important Gallium services:

```cpp
struct GameSubsystems
{
    ga::platform::Platform& platform;
    ga::gpu::Device& gpu;
    ga::assets::AssetManager& assets;
    ga::audio::AudioSystem& audio;
};
```

This is the cleanest integration point for gameplay code.

## What belongs in Calcium3D?

Use Calcium3D for:

```text
Game lifecycle
Scenes
GameObjects
Components
Physics integration
Game-oriented rendering
Game-oriented audio
```

Use Gallium for:

```text
Platform
Input
GPU
Assets
Audio backend/service
Renderer infrastructure
Core runtime utilities
```

## Why this boundary is useful

It lets Gallium evolve as a reusable foundation while Calcium3D can provide a much more opinionated game-development experience.

A different application could use Gallium without taking a dependency on Calcium3D's scene/component architecture.
