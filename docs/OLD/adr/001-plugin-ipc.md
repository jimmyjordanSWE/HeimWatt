# ADR-001: Plugin IPC via Unix Sockets

## Status

Accepted

## Context

Plugins need to communicate with the Core broker. We need a reliable, fast, and secure IPC mechanism that:

1. Works across process boundaries (plugins are forked subprocesses)
2. Supports bidirectional communication
3. Is simple to implement in C
4. Allows for structured message passing (JSON)

**Options Considered**:
- **Shared memory** — Fast but complex synchronization
- **Pipes** — Simple but unidirectional only
- **TCP sockets** — Network overhead, port management
- **Unix domain sockets** — Local only, fast, well-supported

## Decision

Use **Unix domain sockets** with **JSON-over-newline-delimited** messages.

**Protocol**:
- Each message is a single JSON object followed by `\n`
- Messages have a `type` field indicating the operation
- Request-response pattern for queries
- Fire-and-forget for reports

**Socket Location**: Configured via `ipc_socket_path` in config, default `/tmp/heimwatt.sock`

## Consequences

**Positive**:
- Simple implementation using standard POSIX APIs
- Fast (no network stack overhead)
- Built-in access control via filesystem permissions
- Bidirectional communication in single connection

**Negative**:
- Not portable to Windows (would need named pipes)
- Local machine only (no remote plugins without additional work)
- JSON parsing overhead (acceptable for our message volumes)

**Trade-offs accepted**: Local-only is fine for our use case. Windows support is not a priority.
