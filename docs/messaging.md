# Messaging & events

Gallium includes a message/event mechanism so subsystems can communicate without tight compile-time coupling.

A useful example is window resizing.

Instead of:

```text
Platform → Camera
Platform → Renderer
Platform → UI
```

the design can be:

```text
Platform
   ↓
MessageBus
   ├── Camera
   ├── Renderer
   └── UI
```

## Why messages?

Messages are appropriate when:

- multiple systems may care about the same event;
- the sender should not know the receivers;
- the event is asynchronous in the architectural sense;
- the event represents a state transition rather than a continuously queried value.

## Direct calls vs messages

Use direct calls for a value you need now:

```text
Input.IsHeld(...)
```

Use a message for a broadcast event:

```text
window resized
asset reloaded
application event
```

The supplied Calcium3D camera uses the message system for resize notifications.

!!! tip "Keep messages small"
    A message should describe an event or state change, not become a general-purpose object bus containing arbitrary application logic.
