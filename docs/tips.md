# Tips & notes

!!! tip "Learn one layer at a time"
    Gallium is broad. Don't read the GPU backend when you're trying to learn the asset manager. Start from the public subsystem you need and descend only when necessary.

!!! tip "Keep the application loop explicit"
    Gallium is not trying to hide the runtime loop behind a giant framework. Platform events, update, rendering, submission, and presentation are useful architectural boundaries.

!!! tip "Use typed assets"
    `AssetManager::Load<T>()` is preferable to passing raw filenames through unrelated systems. The resource type stays explicit.

!!! tip "Use the renderer abstraction when it fits"
    Raw GPU access is powerful, but `RendererBase` exists to prevent every application from rebuilding the same frame-level machinery.

!!! tip "Use messages for broadcasts"
    Resize and similar events are good message candidates. Input state is usually better queried from the input service.

!!! warning "GPU lifetime matters"
    GPU resources are not ordinary CPU objects. Pay attention to synchronization and to the lifetime of command encoders, images, buffers, and presentation resources.

!!! warning "Don't confuse asset paths with OS paths"
    Paths such as `/assets/model.glb` are application resource paths. Keep platform-specific filesystem details inside the asset layer.

!!! note "Gallium is intentionally below Calcium3D"
    If a feature needs `GameObject`, `GameScene`, or gameplay lifecycle semantics, it probably belongs in Calcium3D rather than Gallium.

!!! note "Public headers are the contract"
    The source implementation is useful for understanding current behavior, but code under `src/` should not automatically be considered stable API.

## A useful debugging strategy

When something goes wrong, identify the layer first:

```text
Window/input problem?     → Platform
Asset not loading?        → Assets
GPU validation/error?     → GPU
Frame looks wrong?        → Renderer / shader
Sound not playing?        → Audio
Objects not reacting?     → Calcium3D
```

This usually narrows the search dramatically.
