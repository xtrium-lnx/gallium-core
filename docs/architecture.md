# Architecture

Gallium is best understood as a collection of cooperating subsystems rather than one monolithic engine.

```text
                     Application
                         │
       ┌─────────────────┼──────────────────┐
       │                 │                  │
   Platform            Assets             Audio
       │                 │                  │
       └─────────────── GPU / Render ───────┘
                         │
                      Utilities
```

## Platform

Owns the relationship with the host operating system:

- application/window lifetime
- input
- platform messages/events
- window size and related state

## GPU

Provides explicit graphics abstractions:

- device
- command encoders
- buffers
- images
- samplers
- shader/pipeline objects
- synchronization
- presentation

The GPU layer is lower-level than `RendererBase`.

## Rendering

The rendering layer provides a renderer-oriented abstraction on top of GPU resources.

The key idea is to let an application submit rendering data such as mesh instances, lights, cameras, and environment information without having to build the entire frame graph itself.

## Assets

The asset manager is the bridge between application paths and typed runtime resources.

It is deliberately generic enough to support different resource types.

## Audio

The audio subsystem follows the same general principle: applications work with a higher-level system/source abstraction while the backend deals with actual playback.

## Messaging

Messaging decouples subsystems that need to react to application/runtime events.

Window resize is a useful example: a camera or renderer can react to a platform message without the platform knowing about cameras.

## Core

Properties, reflection, serialization, logging, hashing, and other utilities support the other subsystems.

!!! note "Dependency direction"
    Higher-level Gallium services depend on lower-level facilities. The platform should not know about game objects; the asset manager should not know about Calcium3D scenes; and the renderer should not need to know gameplay concepts.

## Calcium3D relationship

Calcium3D builds a game-oriented composition model on top:

```text
Gallium:
  Platform → Input / Window
  GPU      → Device / Commands / Resources
  Assets   → Mesh / Image / Sound
  Audio    → Sources / Listener
  Render   → RendererBase

Calcium3D:
  Game
  GameScene
  GameObject
  Component
  DefaultRenderer
```
