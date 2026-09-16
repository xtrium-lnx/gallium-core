# Rendering API

The rendering API provides higher-level frame/render abstractions above the GPU layer.

### `include/gallium/render/rendererbase.h`

`CommandEncoder`, `Image`, `Terrain`, `ParticleSystemRegistry`, `ELightType`, `RendererBase`, `MeshInstance`, `LightData`

### `include/gallium/render/mesh.h`

`Vfs`, `Device`, `Buffer`, `ComputePipeline`, `RenderEncoder`, `TransferEncoder`, `ShaderStructInstance`, `Mesh`, `EVertexStream`, `Interpolation`, `Path`, `ASGeometry`, `VertexStream`, `Joint`, `AnimationSampler`, `AnimationChannel`, `AnimationClip`, `Skin`, `Primitive`, `MeshInstance`

### `include/gallium/render/rendergraph.h`

`ERGBindingFlags`, `RenderGraph`, `PassBuilder`, `RGBinding`, `RGImageInfo`, `RGLoadOp`, `Access`, `Internal`

### `include/gallium/render/materiallibrary.h`

`Device`, `Buffer`, `Image`, `MaterialLibrary`, `MaterialValue`, `Material`


## API shape

Rendering resources describe what should be drawn; the GPU API describes how the work is encoded.

See [Rendering](../rendering.md) for the intended boundary.
