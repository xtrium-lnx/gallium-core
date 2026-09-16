# Platform & input

The platform subsystem is the application's connection to the operating system and windowing environment.

## Platform responsibilities

At the Gallium level, the platform is responsible for things such as:

- creating/owning the application window
- processing operating-system events
- exposing input state
- publishing platform messages
- exposing window dimensions needed by rendering

The exact native backend is an implementation detail.

## Input

Applications can query input through the platform's input service.

Calcium3D's example uses the pattern:

```cpp
subsystems.platform.Input().IsHeld(...);
```

This is useful because game/application code does not need to know how the keyboard is represented by the native OS.

Typical input questions are:

```text
Is a key held?
Was a key pressed?
Was a key released?
```

The precise available methods and key enums should be checked against the public `ga::platform::Input` declaration in the API reference.

## Window messages

Gallium's messaging layer is used for runtime notifications such as window resize.

This is intentionally decoupled:

```text
OS resize
   ↓
Platform
   ↓
message
   ↓
Camera / Renderer / UI
```

A subsystem can subscribe to the message it needs without the platform carrying knowledge of that subsystem.

!!! tip "Don't poll everything"
    Use direct platform/input queries for input state, but prefer the message system for events such as resize or other broadcast-style notifications.
