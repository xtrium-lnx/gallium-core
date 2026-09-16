# GPU

The GPU subsystem is Gallium's explicit graphics foundation.

Think of it in layers:

```text
Device
 ├── resources
 │    ├── Buffer
 │    ├── Image
 │    ├── Sampler
 │    └── ...
 │
 ├── pipelines / shaders
 │
 └── command encoding
      └── submission / synchronization / presentation
```

## Device

`ga::gpu::Device` is the central GPU object.

Higher-level code receives it from application initialization and uses it to create or access GPU resources and encode work.

## Images

Images represent GPU-accessible image resources.

They are used for:

- textures
- render targets
- depth/stencil targets
- presentation images
- environment maps

The asset manager can also load GPU images as typed assets:

```cpp
auto image =
    ga::assets::AssetManager::Global()
        .Load<ga::gpu::Image>("/assets/environment.hdr");
```

## Buffers

Buffers hold GPU-accessible structured/raw data.

They are the normal mechanism for passing vertex, index, uniform, and storage data to GPU pipelines.

## Command encoding

Gallium separates **describing GPU work** from the higher-level application logic.

The command encoder is supplied to rendering code so a frame can be assembled into GPU commands.

This separation is particularly important when integrating a renderer into Calcium3D: the game scene gathers rendering information while the renderer emits commands.

## Synchronization

GPU resources and presentation are explicitly synchronized.

The presence of semaphore/fence types in the API is intentional: frame ownership and GPU execution are asynchronous.

!!! warning "Don't assume GPU work is synchronous"
    Submitting or encoding a command does not mean the GPU has completed it. Respect the synchronization objects required by the API when reusing resources or presenting frames.

## Shaders

Gallium's GPU layer supports shader/pipeline resources. The supplied Calcium3D build also compiles Slang shaders through its build system.

For most applications, shader management should remain on the renderer/material side rather than leaking raw shader setup into gameplay code.
