# Design Study: Blocking & Concurrency in Core

> **Status**: Draft  
> **Date**: 2026-01-20

## The Problem

Current Core has some blocking patterns that could stall the event loop:
- Large JSON parsing
- Plugin health checks (waitpid)
- Database writes

---

## Options

### Option 1: Worker Thread Pool

**Pattern**: Main thread dispatches work to pool, continues processing.

```c
typedef struct {
    pthread_t threads[4];
    queue_t work_queue;
    pthread_mutex_t lock;
    pthread_cond_t cond;
} thread_pool_t;

void pool_submit(thread_pool_t *pool, void (*fn)(void*), void *arg);
```

**Use case**: JSON parsing, DB writes

```c
// Main event loop
case POLLIN:
    ssize_t n = read(fd, buf, sizeof(buf));
    // Submit parsing to worker
    pool_submit(&pool, parse_and_handle, duplicate_buffer(buf, n));
    // Main thread immediately returns to epoll_wait
```

**Pros**:
- True parallelism for CPU-bound work
- Event loop stays responsive

**Cons**:
- Thread synchronization complexity
- Need thread-safe queues for results

---

### Option 2: SIGCHLD for Plugin Health

**Pattern**: Kernel notifies via signal when child dies.

```c
void sigchld_handler(int sig) {
    // Reap zombie and set flag
    int status;
    pid_t pid;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        mark_plugin_dead(pid);  // Lock-free atomic flag
    }
}

// Setup
struct sigaction sa = {.sa_handler = sigchld_handler};
sigaction(SIGCHLD, &sa, NULL);
```

**Main loop**:
```c
// After epoll_wait returns
if (plugin_died_flag) {
    handle_plugin_deaths();  // Now safe to do work
}
```

**Pros**:
- No polling overhead
- Immediate notification
- Zero latency for plugin crashes

**Cons**:
- Signal handlers are tricky (async-signal-safe only)
- Must use pipe or eventfd to wake epoll

---

### Option 3: signalfd (Linux-specific)

**Pattern**: Receive signals via file descriptor.

```c
sigset_t mask;
sigemptyset(&mask);
sigaddset(&mask, SIGCHLD);
sigprocmask(SIG_BLOCK, &mask, NULL);

int sfd = signalfd(-1, &mask, SFD_NONBLOCK);
// Add sfd to epoll set
```

**Then in event loop**:
```c
if (events[i].data.fd == sigchld_fd) {
    struct signalfd_siginfo info;
    read(sigchld_fd, &info, sizeof(info));
    // Handle dead plugin
}
```

**Pros**:
- Integrates cleanly with epoll
- No signal handler races
- Synchronous processing

**Cons**:
- Linux-only (not portable to macOS)

---

## Recommendation

**Use both Option 1 and Option 3**:

| Task | Strategy |
|------|----------|
| JSON parsing | Thread pool (4 workers) |
| DB writes | Thread pool or async queue |
| Plugin health | signalfd + epoll (Linux), SIGCHLD handler fallback (macOS) |

### Implementation Order

1. **Add signalfd for SIGCHLD** — immediate benefit, low effort
2. **Add thread pool** — requires careful queue design
3. **Profile first** — measure actual latencies before over-engineering

---

## Thread Pool Sizing

```
Pool size = Number of CPU cores (for CPU-bound)
         or 2×cores (for IO-bound with blocking calls)
```

For HeimWatt: **4 workers** is sufficient. JSON parsing is fast; this handles burst of concurrent requests.
