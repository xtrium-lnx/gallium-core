# GPU API

The GPU API contains the low-level graphics resource and command abstractions.

### `include/gallium/assets/imagedata.h`

`ImageData`

### `include/gallium/gpu/shadermodule_reflection.h`

`ShaderStructInstance`, `ShaderStructArrayInstance`, `ShaderMemberProxy`, `ShaderStructDesc`, `ShaderMemberDesc`

### `include/gallium/gpu/shadermodule.h`

`Device`, `ShaderModule`, `ShaderStructDesc`, `Impl`

### `include/gallium/gpu/buffer.h`

`Device`, `ShaderStructInstance`, `ShaderStructArrayInstance`, `TransferEncoder`, `Buffer`, `DescriptorIndex`, `ShaderStructDesc`, `BufferInfo`, `Impl`

### `include/gallium/gpu/computepipeline.h`

`Device`, `ComputePipeline`, `ComputePipelineInfo`, `Impl`

### `include/gallium/gpu/device.h`

`Platform`, `CommandBuffer`, `DescriptorRegistry`, `Image`, `ShaderCache`, `Device`, `DeviceCaps`, `Impl`, `AcquiredImage`

### `include/gallium/gpu/commandbuffer.h`

`Device`, `AccelerationStructure`, `Buffer`, `Image`, `GraphicsPipeline`, `ComputePipeline`, `RaytracingPipeline`, `ShaderStructInstance`, `CommandEncoder`, `RenderEncoder`, `ComputeEncoder`, `RaytracingEncoder`, `TransferEncoder`, `CommandBuffer`, `BeginRenderingInfo`, `ColorAttachment`, `DepthAttachment`, `BeginComputeInfo`, `BeginRayTracingInfo`, `TransitionInfo`, `Impl`, `BufferCopyInfoInternal`

### `include/gallium/gpu/image.h`

`Device`, `Image`, `DescriptorIndex`, `MipData`, `ImageInfo`, `ImageInfoEx`, `ExistingImageInfo`, `Impl`

### `include/gallium/gpu/pipeline.h`

`PipelineStageDesc`

### `include/gallium/gpu/raytracingpipeline.h`

`Device`, `ERaytracingShaderGroupType`, `RaytracingPipeline`, `RaytracingShaderGroupDesc`, `RaytracingPipelineInfo`, `Impl`, `SBTTable`, `SBTInfo`

### `include/gallium/gpu/shadercache.h`

`Vfs`, `Device`, `ShaderModule`, `ShaderCache`, `Impl`

### `include/gallium/gpu/graphicspipeline.h`

`Device`, `GraphicsPipeline`, `VertexBindingDesc`, `VertexAttributeDesc`, `VertexInputDesc`, `AttachmentFormatsDesc`, `ColorBlendDesc`, `DepthState`, `GraphicsPipelineInfo`, `Impl`


## API shape

The important conceptual hierarchy is:

```text
Device
 ├── resources
 ├── shaders/pipelines
 └── command encoding
```

See [GPU](../gpu.md) for usage guidance.
