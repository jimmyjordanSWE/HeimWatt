# HeimWatt OS/Kernel Implementation Patterns

> **Status**: Implementation Reference
> **Context**: HeimWatt runs as a highly privileged process supervisor (Core) in a Dockerized environment (Home Server). It effectively acts as a "Kernel" for energy management.

## 1. Systemd Integration (The Init System)
Even in Docker, `systemd` interactions (or Docker healthchecks acting similarly) are robust.

*   **Watchdog Integration (`sd_notify`)**:
    *   **Mechanism**: The main `epoll` loop pings the watchdog every `N/2` seconds.
    *   **Benefit**: If the main thread deadlocks (e.g., IPC hang, lock inversion), the supervisor kills and restarts the container/service.
    *   **Implementation**: Use `libsystemd` or simple socket write to `NOTIFY_SOCKET`.

*   **Socket Activation**:
    *   **Mechanism**: Supervisor (systemd/Docker) creates the listening FD (port 80, IPC socket) and passes it to HeimWatt.
    *   **Benefit**: Zero-downtime restarts. Clients stay connected while the process swaps out.

## 2. Process Isolation (Namespaces)
`plugin_mgr.c` currently uses raw `fork()`. To secure the host:

*   **Linux Namespaces (`unshare`)**:
    *   **Mount Namespace**: `CLONE_NEWNS`. Remount `/` as read-only for plugins. Only bind-mount the plugin's config dir and `/tmp` as writable.
    *   **Network Namespace**: `CLONE_NEWNET`. Create an empty network namespace for local plugins (physics, device logic) so they **cannot** access the internet or local LAN. Only IPC socket is available.
    *   **Implementation**: Call `unshare()` in the child process before `exec()`.

## 3. Resource Control (Cgroups v2)
Prevent "Noisy Neighbor" plugins from starving the Core or other critical services (like Jellyfin).

*   **Mechanism**:
    *   Create a cgroup subtree: `/sys/fs/cgroup/heimwatt/plugins/<id>/`
    *   Set `cpu.max` (e.g., "10000 100000" = 10% CPU limit).
    *   Set `memory.max` (e.g., "128M").
*   **Benefit**: A memory leak in a Python plugin triggers OOM-kill for *that plugin only*, not the whole system.

## 4. Capability Management (Security)
HeimWatt Core should not run as full `root`.

*   **Principle**: Least Privilege.
*   **Required Capabilities**:
    *   `CAP_NET_BIND_SERVICE`: To bind port 80/443 (if not behind reverse proxy).
    *   `CAP_SYS_TTY_CONFIG`: Maybe needed for configuring Serial/P1 ports.
*   **Implementation**: `setcap cap_net_bind_service=+ep build/heimwatt`.

## 5. High-Performance Event Loop
*   **Signalfd**: Handle `SIGTERM`, `SIGCHLD` (plugin death) via the `epoll` loop. Eliminates racy signal handlers.
*   **Timerfd**: Use nanosecond-precision file-descriptor timers for the 1-minute logic tick.
*   **Eventfd**: For high-speed inter-thread notifications (e.g., "Data Store updated").

## 6. Docker Considerations
Since the user runs a "Home Server" (Jellyfin, backups, HeimWatt):

*   **Passthrough**: P1 USB cables need `--device /dev/ttyUSB0`.
*   **State**: Map `~/.heimwatt` to a volume.
## 7. Next-Gen I/O (`io_uring`)
For true "OS-level" performance, replacing `epoll` with `io_uring` reduces syscall overhead.

*   **Mechanism**: Shared ring buffers (Submission Queue, Completion Queue) between kernel and userspace.
*   **Usage**:
    *   Batch submission of socket reads/writes.
    *   Async file I/O (logging, database WAL).
*   **Verdict**: Complexity is high. Valid for V2 if `epoll` becomes a bottleneck (unlikely at <10k ops/sec), but excellent for learning kernel interaction.

## 8. Zero-Copy IPC (Fast Mem Copy)
Sending large schedule blobs (JSON) over sockets involves packing/unpacking and copying.

*   **Mechanism**:
    *   **`memfd_create`**: Create a RAM-backed file.
    *   **`mmap`**: Map it into memory.
    *   **Socket Passing**: Send the file descriptor to the plugin via Unix socket (`SCM_RIGHTS`).
*   **Security Note**: Shared memory is "unsecure" by default (race conditions, buffer overflow).
    *   **Mitigation**: Use **Sealing** (`F_SEAL_WRITE`) to make the data read-only for the consumer (Plugin) once written by the Producer (Core).
## 9. MacOS / Apple Silicon Optimizations (Mac Mini M1)
Since the M1 Mac Mini is a target (ARM64, Darwin kernel), we implement platform-specific bridging.

*   **I/O Multiplexing (`kqueue`)**:
    *   **Equivalent to**: `epoll` + `signalfd` + `timerfd`.
    *   **Mechanism**: `kqueue` filters (`EVFILT_READ`, `EVFILT_SIGNAL`, `EVFILT_TIMER`).
    *   **Optimization**: Use `EV_CLEAR` (Edge Triggered) for high concurrency.

*   **Init System (`launchd`)**:
    *   **Equivalent to**: `systemd`.
    *   **Socket Activation**: `launchd` creates sockets declared in `plist`. Retrieve via `launch_activate_socket()`.
    *   **KeepAlive**: Configured in `~/Library/LaunchAgents/se.heimwatt.core.plist`.

*   **Hardware Acceleration (Solver)**:
    *   **Accelerate Framework**: The M1 has powerful AMX (Matrix) units.
    *   **Optimization**: Link the Solver (if C/C++) against `Accelerate.framework` (search path `/System/Library/Frameworks/Accelerate.framework`) for vecLib BLAS/LAPACK. This dramatically speeds up matrix operations in MPC/MILP solvers.

*   **Energy Efficiency (QoS)**:
    *   **Mechanism**: `pthread_set_qos_class_self_np()`.
    *   **Usage**: Run the Solver thread as `QOS_CLASS_USER_INITIATED` (Performance cores). Run logging/background tasks as `QOS_CLASS_BACKGROUND` (Efficiency cores).


