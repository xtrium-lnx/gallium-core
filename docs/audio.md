# Audio

Gallium provides the lower-level audio system used by Calcium3D.

## Audio system

`ga::audio::AudioSystem` is the service-level entry point.

An application generally creates the system once and then creates/configures audio sources through it.

Calcium3D exposes the same service through:

```cpp
GameSubsystems::audio
```

## Sources

An audio source represents a playable sound.

At the engine level, Calcium3D's `AudioSource` component wraps this concept and adds scene-transform behavior.

The useful conceptual split is:

```text
Sound asset
    ↓
Audio source
    ↓
Audio system
    ↓
audio backend/device
```

## Listener

A listener represents the point from which spatial audio is heard.

Calcium3D's `AudioListener` component updates Gallium's listener from a `GameObject` transform.

## Spatialization

A spatialized source needs a source position and a listener position/orientation.

Gallium owns the audio-side mechanics; Calcium3D decides how those positions map to scene objects.

!!! note "Gallium is not the game audio component layer"
    Gallium knows about audio playback. Calcium3D's `AudioSource` adds game-object/component lifecycle and transform integration on top.
