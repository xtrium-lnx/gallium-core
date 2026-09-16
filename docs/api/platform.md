# Platform API

The platform API is the application/OS boundary.

Typical responsibilities include window lifetime, input, event processing, and platform messages.

### `include/gallium/core/inputmessages.h`

`Enum`

### `include/gallium/core/inputtranslator.h`

`Window`, `EInputTranslationMode`, `EInputKeyEventType`, `EInputScrollDirection`, `InputTranslator`, `InputMouseState`, `Internal`, `Enum`

### `include/gallium/platform/input.h`

`Platform`, `EKey`, `EMouseButton`, `EMouseMode`, `Input`, `Impl`

### `include/gallium/platform/platform.h`

`Vfs`, `Input`, `Timer`, `Platform`, `PlatformInfo`, `Impl`, `Surface`


## Typical use

```cpp
platform.Input().IsHeld(...);
```

The platform object is normally created once and kept alive for the application lifetime.

See [Platform & input](../platform.md) for the conceptual guide.
