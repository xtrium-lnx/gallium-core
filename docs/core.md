# Core utilities

Gallium's core utilities support the rest of the library.

Important concepts visible from the public API include properties, hashing, reflection/serialization, logging, and common data types.

## Properties

`ga::core::Property<T>` is an engine-aware value wrapper.

It appears throughout Calcium3D:

```cpp
camera->Fov = 75.0f;
float fov = camera->Fov();
```

A property is useful when a value needs more than a plain C++ field—for example:

- change tracking
- editor/inspector integration
- serialization
- callbacks

## Hashing

Gallium uses hashed identifiers in places where runtime lookup is useful.

Calcium3D's component factory, for example, uses `string_hash_t` for component type identity.

The general pattern is:

```text
human-readable name
        ↓
stable hash
        ↓
fast runtime lookup
```

## Serialization & reflection

The property/component system is designed to make runtime state discoverable and serializable.

This is what lets higher-level tools inspect component properties without every component needing a completely separate editor implementation.

## Logging

Gallium provides logging facilities used by the engine/application stack.

Prefer the library's logging abstraction over direct `printf`/`std::cout` calls when a message is part of runtime diagnostics.

## Utilities are infrastructure

Core utilities should generally stay invisible to application architecture.

If a gameplay class becomes dominated by serialization, hashing, or property plumbing, that is often a sign that the infrastructure should be wrapped at a higher level.
