# GalliumCore

** GalliumCore is a low-level, opinionated foundation for building real-time applications and game engines.**

It provides a modern C++ abstraction over the fundamental systems required by a renderer and runtime: GPU resources and command submission, synchronization, shader interfaces, rendering infrastructure, platform services, assets, and audio.

GalliumCore is built around a simple idea: **A real-time engine should not have to choose between a high-level architecture and direct control over the GPU.**

The purpose of GalliumCore is to provide the layer where those two meet. As such, is built around a **fully bindless GPU resource model**. Images are exposed to shaders through descriptor indices, while buffers are accessed through **buffer device addresses (BDA)**. Resource binding is therefore decoupled from individual draw or dispatch calls, allowing rendering systems to build persistent resource registries and pass lightweight handles or indices through GPU data.

GalliumCore deliberately does **not** attempt to be a complete game engine. It is the layer underneath one.

---

## Overview

The project is organized into a small number of focused subsystems:

```text
ga
├── assets    Asset management and loading
├── audio     Audio playback, mixing and effects
├── gpu       GPU hardware abstraction layer
├── platform  Platform, input, timing and virtual filesystem
└── render    Rendering infrastructure built on top of gpu
```

The `gpu` subsystem forms the core of the library. `render` builds on top of it without exposing the underlying graphics API to its users.

---

## Design goals

### Low-level

GalliumCore is intended to sit close to the hardware.

GPU resources, command encoding, synchronization, pipeline state and shader interfaces are explicit concepts rather than implementation details hidden behind a high-level scene API.

The library is suitable for applications that need direct control over GPU workloads and resource lifetime.

### Bindless model

Resource binding is a fundamental part of the architecture rather than an optional rendering technique.

GalliumCore uses a **fully bindless resource model**:

* **Images** are referenced from shaders using descriptor indices.
* **Buffers** are accessed using **buffer device addresses (BDA)**.
* Resource identity is therefore independent from individual draw and dispatch calls.
* GPU-side data can directly reference resources without rebuilding conventional per-pass binding sets.

Conceptually, instead of a rendering operation describing *which resources are bound to which slots*, the application passes resource identifiers as data:

```text
CPU
 │
 ├── Image ──────────────► Descriptor Registry
 │                              │
 │                         DescriptorIndex
 │                              │
 │                              ▼
 │                           Shader
 │
 └── Buffer ──────────────► Buffer Device Address
                                │
                                ▼
                             Shader
```

This model is particularly well suited to modern renderers using large persistent resource pools, GPU-driven rendering, material systems, render graphs and ray tracing.

It also strongly influences the rest of the architecture: descriptor management, shader interfaces, material representation and rendering data are designed around persistent GPU-visible resource identity rather than traditional per-draw binding.

### Opinionated

GalliumCore is **not** intended to be a lowest-common-denominator abstraction.

It does not try to make every graphics API look identical, nor does it expose every feature of every backend through a generic interface.

Instead, GalliumCore defines a coherent programming model and expects backends to implement that model.

This makes the API opinionated by design.

### Modern C++

The public API makes extensive use of modern C++ features:

* Strongly typed enumerations and descriptors
* RAII and explicit resource lifetime
* Templates where compile-time information is useful
* Type-safe handles and indices
* Value-oriented configuration structures
* Generic interfaces for extensible systems

### Explicitness

Expensive or asynchronous operations should not happen accidentally.

Resource creation, command recording, synchronization, pipeline construction and descriptor usage are represented explicitly in the API.

At the same time, implementation details that are irrelevant to the user are kept behind the abstraction boundary.

### Portability without lowest-common-denominator APIs

The same application should be able to target multiple platforms and graphics backends without having to duplicate its rendering architecture.

Portability is achieved by defining common **semantics**, not by pretending that all APIs work the same way.

---

# Architecture

## `ga::gpu`

The GPU subsystem is the hardware abstraction layer.

It provides the primitives required to build a modern explicit renderer:

```text
Device
 ├── Buffers
 ├── Images
 ├── Pipelines
 │    ├── Graphics
 │    ├── Compute
 │    └── Ray tracing
 ├── Shader modules
 ├── Descriptors
 ├── Acceleration structures
 └── Synchronization
```

Command submission is exposed through command buffers and specialized encoders:

```text
CommandBuffer
 ├── CommandEncoder
 ├── TransferEncoder
 ├── ComputeEncoder
 └── RaytracingEncoder
```

The API also models resource transitions, pipeline stages, access types, image layouts and synchronization primitives explicitly.

### Resource model

The GPU API is built around persistent resources rather than transient binding state.

Images are registered with the descriptor system and represented to shaders by `DescriptorIndex`. Buffers expose their device address directly through the API.

This allows GPU data structures to contain references to resources without requiring the CPU to construct traditional descriptor sets for every rendering operation.

### Ray tracing

Ray tracing is a first-class part of the GPU abstraction rather than a backend-specific extension.

The API includes concepts such as:

* Acceleration structures
* BLAS/TLAS construction
* Geometry and instance descriptions
* Ray-tracing pipelines
* Shader groups
* Ray-tracing command encoding

This allows higher-level rendering systems to use ray tracing without depending directly on a particular graphics API.

### Shaders

Shader modules and their interfaces are represented explicitly.

Shader reflection information can be used to describe:

* Shader members
* Shader structures
* Structured arrays
* Variable types
* Descriptor interfaces

The shader interface is designed around the same bindless resource model as the C++ API.

---

## `ga::render`

`ga::render` provides rendering infrastructure built on top of `ga::gpu`.

It deliberately stops short of becoming a complete scene or game framework.

The centerpiece is the **render graph**:

```text
RenderGraph
    │
    ├── PassBuilder
    ├── Resources
    ├── Attachments
    ├── Transitions
    └── GPU execution
```

The render layer also provides common rendering data and infrastructure for:

* Materials
* Meshes and vertex streams
* Lighting
* Animation and skinning
* Fonts and glyphs
* Post-processing
* Rendering passes

The intent is to provide reusable renderer infrastructure while leaving application-specific rendering architecture to the user.

---

## `ga::platform`

The platform layer isolates operating-system and window-system concerns from the rest of the library.

It currently covers:

* Platform initialization
* Input
* Keyboard and mouse state
* Input callbacks
* Timers
* Virtual filesystem
* File access and VFS providers

The rest of the library should not need to know whether it is running on Windows, Linux, or another supported platform.

---

## `ga::assets`

The asset subsystem provides lightweight infrastructure for loading and managing application resources.

The central abstraction is the `AssetManager`, with asset loading delegated to type-specific loaders.

```cpp
template<typename T>
class IAssetLoader;
```

This keeps asset management separate from individual file formats and allows applications to provide their own asset types and loaders.

---

## `ga::audio`

The audio subsystem provides a similarly small abstraction for runtime audio.

It includes:

* Audio sources
* Sound resources
* Mixer buses
* Audio effects
* Source state management
* Handles for audio resources

Effects such as low-pass, high-pass and gain processing are represented independently from the source and mixer infrastructure.

---

# What GalliumCore is not

GalliumCore intentionally does not provide:

* An ECS
* A scene graph
* Gameplay systems
* Physics
* Networking
* Game-specific input mapping
* A scripting language
* A complete material editor
* A complete asset pipeline
* A high-level entity/component API
* A game-specific application framework

These systems can be built **on top of** GalliumCore, but they are not part of its core abstraction.

The boundary is intentional:

> **GalliumCore provides the machinery, but it's up to the application to decide what to build with it.**

---

# Intended use

GalliumCore is designed for developers building:

* Custom game engines
* Real-time rendering applications
* Demoscene productions
* Visualization software
* Graphics experiments and research projects
* Specialized interactive applications

It is particularly useful when an application needs more control than a conventional high-level engine provides, while still benefiting from a portable and structured API.

---

# Status

GalliumCore is an actively developed engine foundation.

The API should currently be considered **evolving** rather than a stable long-term ABI.

The project prioritizes architectural correctness and a coherent programming model over backwards compatibility with experimental APIs.

---

# License

See [`LICENSE`](LICENSE) for the applicable license.

---

# Contributing

GalliumCore is developed as a focused, opinionated codebase, and its architecture is deliberately kept under tight control.

The project does not currently accept unsolicited code contributions or pull requests. Contributions may be invited when a change aligns with the project's architectural direction and long-term goals.

Bug reports, technical discussions, and well-reasoned suggestions are nevertheless welcome. If you have an idea that could improve GalliumCore, open an issue or start a discussion before investing time in an implementation.

This is not intended to discourage participation, but to keep the core API coherent and prevent the project from gradually becoming a collection of individually useful abstractions without a consistent design.
