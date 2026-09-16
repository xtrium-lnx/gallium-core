# Rendering

Gallium's rendering subsystem sits between raw GPU operations and application-level rendering.

## `RendererBase`

The renderer abstraction is designed around frame-oriented input.

A renderer can receive things such as:

```text
geometry
lights
environment
camera
```

and turn them into GPU commands.

Calcium3D's `DefaultRenderer` is one implementation of this idea.

## Mesh instances

The rendering API represents geometry as reusable mesh resources plus per-instance state such as transforms.

This gives a useful split:

```text
Mesh asset
    ↓
reusable geometry

MeshInstance + transform
    ↓
one object in the scene
```

## Camera

A renderer needs a view and projection transform.

Calcium3D supplies those through its camera component, but Gallium itself remains renderer-oriented rather than scene-oriented.

## Lights

Gallium's renderer-facing light data describes the physical/rendering parameters. A higher-level engine decides where those lights come from and how they are organized.

## Why keep `RendererBase` separate from the GPU?

Because these solve different problems.

GPU:

> “How do I encode and submit graphics work?”

Renderer:

> “What graphics should this frame contain?”

That boundary is one of the most useful architectural seams in Gallium.

!!! tip "Renderer first, raw GPU second"
    If your application only needs conventional mesh/light/camera rendering, work through the renderer abstraction. Drop down to `ga::gpu` when you actually need custom GPU behavior.
