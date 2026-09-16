# Gallium

**Gallium is the foundation layer.**

It provides the platform, graphics, assets, audio, and utility abstractions that a higher-level engine can build on.

If Calcium3D is the game engine, Gallium is the reusable runtime underneath it.

image_group{"layout":"carousel","aspect_ratio":"16:9","query":["game engine architecture rendering assets GPU diagram","modern game engine rendering pipeline"],"num_per_query":1}

## Start here

| Goal | Read |
|---|---|
| Build a small Gallium application | [First application](getting-started.md) |
| Understand the library boundaries | [Architecture](architecture.md) |
| Create a window and process input | [Platform & input](platform.md) |
| Work with the GPU | [GPU](gpu.md) |
| Use the renderer abstraction | [Rendering](rendering.md) |
| Load resources | [Assets](assets.md) |
| Play sound | [Audio](audio.md) |
| Communicate between systems | [Messaging & events](messaging.md) |
| Understand properties/serialization | [Core utilities](core.md) |
| Integrate with Calcium3D | [Calcium3D integration](calcium3d.md) |
| Find a practical gotcha | [Tips & notes](tips.md) |

## The central idea

Gallium is organized around **services and resources**.

The platform gives you a runtime environment. The GPU gives you device and command abstractions. Assets give you a way to locate/load resources. Rendering and audio build useful higher-level services on top.

That separation makes Gallium suitable as a foundation for several applications, not only Calcium3D.

## A note about this documentation

This is intentionally not a mechanical Doxygen dump.

The goal is to answer:

> “What is this subsystem for, how do I use it, and what should I watch out for?”

The API pages then provide a compact reference when you already know what you're looking for.
