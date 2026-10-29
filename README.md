# microjail
A minimal Linux container runtime built from scratch in C++ (namespaces, chroot, cgroups v2).# MicroJail

A minimal Linux container runtime written in C++17 on top of raw kernel
primitives (namespaces, pivot_root, cgroups v2). No Docker, runc or other
container library is used.

Course project for Unix Programming Lab (24B15CS214), JIIT Noida.

## What it does

`microjail run` starts a process that has:

- its own **hostname** (UTS namespace)
- its own **process tree**, where the command is PID 1 (PID namespace)
- its own **mount table** and a private `/proc` (mount namespace)
- its own **IPC objects** (IPC namespace)
- a **confined root filesystem** (`pivot_root`, with `chroot` as fallback)
- **memory and CPU limits** (cgroups v2)

It also logs the container lifecycle, tracks running containers, and provides
`list` and `stop` commands.

## Requirements

- Linux with cgroups v2 (Ubuntu 22.04 or 24.04 recommended)
- g++ with C++17, make, wget
- root privileges (`sudo`)

## Build

    git clone git@github.com:Maharshi-Paul/microjail.git
    cd microjail
    make
    ./scripts/fetch_rootfs.sh             # downloads Alpine's minimal rootfs
    ./scripts/install_test_programs.sh    # optional: memory/CPU hog test programs

## Usage

    sudo ./microjail run [--name N] [--mem 50M] [--cpu 50] [--hostname H] <rootfs> <cmd> [args...]
    sudo ./microjail list
    sudo ./microjail stop <name>

Examples:

    sudo ./microjail run --name box1 --mem 100M --cpu 30 --hostname mybox ./rootfs /bin/sh
    sudo ./microjail run ./rootfs /bin/echo hello
    sudo ./microjail run --name hog --mem 50M ./rootfs /usr/local/bin/memhog

Defaults (50 MB, 50% of one CPU) come from `config/limits.conf` and can be
overridden on the command line. Events are written to `logs/microjail.log`.

## Tests

    sudo tests/test_namespaces.sh

Runs 13 checks (isolation, limits, OOM kill, cleanup, list/stop, input
validation) and prints PASS or FAIL for each.

## How it works

See [docs/architecture.md](docs/architecture.md) for the design, the system
calls used, and a diagram of the startup sequence.

## Limitations

- The container shares the host network (no network namespace).
- No user namespace, seccomp filter or capability dropping: the process inside
  runs as root with full capabilities, so this is a learning tool, not a security
  boundary.
- No image layering (OverlayFS); the rootfs is a plain directory.
- Run it from the repository root (the log and config paths are relative).

## License

MIT, see [LICENSE](LICENSE). Contributors are listed in
[CONTRIBUTORS.md](CONTRIBUTORS.md).
