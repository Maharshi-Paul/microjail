# Demo script (about 8 minutes)

Setup: Ubuntu VM, repo built (`make`), rootfs fetched,
`./scripts/install_test_programs.sh` run. Two terminals side by side.

## 1. Intro (1 min)
"A container is just a process with a restricted view and restricted
resources. MicroJail builds that from system calls: no Docker."
Show the diagram in docs/architecture.md.

## 2. Isolation (2 min), terminal 1
    sudo ./microjail run --name demo --hostname jail ./rootfs /bin/sh
Inside:
    hostname              # jail (host is different)
    echo $$               # 1
    ps                    # only the container's processes
    cat /etc/os-release   # Alpine; the host is Ubuntu
    ls /                  # only Alpine's folders
    cat /proc/self/mountinfo | wc -l    # 2-3 mounts: host mounts are hidden
Compare with the host: `hostname`, `ps aux | head`.
Namespace IDs differ: `ls -l /proc/self/ns` on host and inside.

## 3. Limits (2 min)
    sudo ./microjail run --name hog --mem 50M ./rootfs /usr/local/bin/memhog
Hog prints 10, 20, 30, 40 MB, then is killed.
    tail -5 logs/microjail.log        # shows the [OOM] line
CPU:
    sudo ./microjail run --name burn --cpu 50 ./rootfs /usr/local/bin/cpuhog
In terminal 2: `top` shows cpuhog near 50%. Stop it from terminal 2 (below).

## 4. Lifecycle (1 min), terminal 2
    sudo ./microjail list
    sudo ./microjail stop burn
Explain why stop takes about 3 seconds (PID 1 ignores SIGTERM without a handler).
    cat /sys/fs/cgroup/microjail/burn/cpu.max    # while running

## 5. Tests and repo (1 min)
    sudo tests/test_namespaces.sh     # 13 passed, 0 failed
Show GitHub: branches, PRs, tags v0.1 to v1.0, README.

## 6. Questions to be ready for
- container vs VM
- why clone() and not fork()
- why is the child PID 1, and what happens when it exits
- why mount /proc again
- chroot vs pivot_root (show the mountinfo difference)
- namespaces vs cgroups
- what Docker adds (images, networking, user namespaces, seccomp, daemon)
- limitations and next steps
