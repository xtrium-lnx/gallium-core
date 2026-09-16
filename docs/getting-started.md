# First application

Gallium is a library rather than a game framework, so the exact application entry point is up to you.

The basic flow is:

```text
create Platform
      ↓
create GPU Device
      ↓
create services/resources
      ↓
poll events
      ↓
encode work
      ↓
present
      ↓
repeat
```

## Platform

The platform layer owns the native application/window environment.

The concrete platform implementation in the supplied repository is selected through Gallium's platform API and provides access to input and window events.

A typical application should keep the platform object alive for the duration of the main loop.

## GPU

Create the GPU device after the platform exists, because the presentation surface/window is part of the relationship between the two.

The device is the entry point for:

- command encoding
- images
- buffers
- synchronization
- pipelines
- shaders
- presentation

## Assets

The global asset manager is the usual way applications request resources:

```cpp
auto mesh =
    ga::assets::AssetManager::Global()
        .Load<ga::render::Mesh>("/assets/model.glb");
```

The important part is that the application asks for a **typed resource**. The asset layer handles locating and loading the resource.

## Main loop

Keep the loop small.

```text
while (running)
{
    platform events
    ↓
    update application state
    ↓
    encode rendering commands
    ↓
    submit/present
}
```

Gallium's platform, GPU, and renderer abstractions are intended to make the contents of that loop explicit.

## Where Calcium3D fits

A Calcium3D application wraps this lifecycle in `c3d::Game::Run`.

You can therefore use Gallium directly for a custom application, or let Calcium3D own the boilerplate and access Gallium through `GameSubsystems`.

!!! tip "Start with the service you actually need"
    You don't need to understand the whole Gallium codebase before using it. Start with the subsystem boundary—platform, GPU, assets, audio, etc.—and only descend into implementation when you need lower-level control.
