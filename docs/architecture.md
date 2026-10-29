# MicroJail architecture

## Overview

A container is an ordinary process that the kernel has been told to treat
specially. MicroJail combines three kernel features:

| Problem | Mechanism | Where |
|---|---|---|
| What can the process see? | Namespaces (PID, UTS, mount, IPC) | `src/namespace_setup.cpp` |
| Which files can it reach? | `pivot_root` (fallback `chroot`) | `src/fs_isolation.cpp` |
| How much can it use? | cgroups v2 | `src/cgroup_manager.cpp` |

Supporting modules: `src/main.cpp` (CLI, orchestration), `src/state.cpp`
(running-container records), `src/process_monitor.cpp` (waiting, usage polling,
signals), `src/logger.cpp` (event log).

## Startup sequence

    microjail run ...                    (parent, runs as root)
      1. parse options, load config/limits.conf defaults
      2. pre-flight: cgroups v2 present? rootfs is a directory? name valid
         and not already running?
      3. pipe()                          "go" channel parent -> child
      4. clone(CLONE_NEWPID | NEWUTS | NEWNS | NEWIPC | SIGCHLD)
                |
                +--> child (new namespaces; sees itself as PID 1)
                       a. close its copy of the pipe's write end
                       b. block on read() until the parent says go
                       c. sethostname()
                       d. mount("/", MS_REC | MS_PRIVATE)
                       e. pivot_root into the rootfs, unmount the old root
                       f. mount a fresh /proc
                       g. execvp(command)
      5. parent: create cgroup, write memory.max / cpu.max,
         add the child's PID to cgroup.procs
      6. parent: log START, write the state file
      7. parent: write one byte to the pipe -> the child may run
      8. parent: monitor loop (waitpid WNOHANG every 0.2 s, usage every 5 s,
         SIGINT/SIGTERM -> SIGKILL the container)
      9. child exits: log EXIT, remove cgroup and state file

The pipe in steps 3, 5 and 7 guarantees that the child is already inside its
cgroup before it executes any user code, so the limits cannot be bypassed.

## System calls and why

| Call | Purpose |
|---|---|
| `clone()` | Create the child and its namespaces in one atomic step (`fork()` cannot) |
| `sethostname()` | Set the container's own hostname (only affects its UTS namespace) |
| `mount(MS_PRIVATE)` | Stop mount events from propagating between host and container |
| `mount(MS_BIND)` | Make the rootfs a mount point, which `pivot_root` requires |
| `pivot_root()` | Swap the root mount; the old root can then be unmounted |
| `umount2(MNT_DETACH)` | Remove the old root so host paths are unreachable |
| `mount("proc")` | A new `/proc` so `ps` only shows the container's processes |
| `execvp()` | Replace the child with the user's command |
| `pipe2(O_CLOEXEC)` | Start synchronisation; the descriptors close on exec |
| `waitpid(WNOHANG)` | Poll for exit while still handling signals and usage |
| `kill()` | `stop`, liveness checks (signal 0), killing on Ctrl+C |
| `sigaction()` | Catch SIGINT/SIGTERM so cleanup always runs |
| `statfs()` | Detect cgroups v2 (`CGROUP2_SUPER_MAGIC`) |

## Resource control

Each container gets `/sys/fs/cgroup/microjail/<name>/`:

- `memory.max`: hard memory limit; exceeding it triggers the OOM killer inside
  the group
- `memory.swap.max = 0`: swap disabled, so the limit is a real wall
- `cpu.max = "<quota> 100000"`: for example `50000 100000` means 50% of one CPU
- `cgroup.procs`: the container's PID is written here

The monitor reads `memory.current`, `memory.peak`, `cpu.stat` and
`memory.events` to log usage and to report OOM kills.

## State and logs

- `/var/run/microjail/<name>.state`: key=value file (pid, start time, limits,
  rootfs) written while the container runs; `list` and `stop` read it.
  Stale files from crashes are detected by checking whether the PID is alive.
- `logs/microjail.log`: timestamped START, USAGE, OOM, INFO and EXIT events.

## Design decisions

- **`pivot_root` over `chroot`:** `chroot` leaves the old filesystem mounted
  (visible in `/proc/self/mountinfo`) and is escapable by a root process.
- **Plain-text state files** instead of a daemon: no background service needed.
- **Parent as supervisor:** `run` stays in the foreground, which makes signal
  handling and cleanup simple.
- **`SIGTERM` is ignored by PID 1:** the kernel drops signals sent to a PID
  namespace's init from outside unless it installed a handler, so `stop`
  escalates to SIGKILL after 3 seconds.

## Known limitations

No network namespace, user namespace, seccomp or capability dropping; no image
layers; root inside the container is root-equivalent on the host for many
operations. These are natural next steps.
