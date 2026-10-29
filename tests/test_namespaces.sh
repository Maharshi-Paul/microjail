#!/bin/bash
# MicroJail checks. Run from the repo root:  sudo tests/test_namespaces.sh
if [ "$EUID" -ne 0 ]; then echo "run with sudo"; exit 1; fi
if [ ! -x ./microjail ] || [ ! -d ./rootfs ]; then
    echo "run from the repo root, after make and fetch_rootfs.sh"; exit 1
fi

MJ=./microjail
PASS=0
FAIL=0

check() {   # check "description" function
    local desc="$1"; shift
    if "$@" >/dev/null 2>&1 </dev/null; then
        echo "PASS  $desc"; PASS=$((PASS+1))
    else
        echo "FAIL  $desc"; FAIL=$((FAIL+1))
    fi
}

t_hostname() { $MJ run --name t-host --hostname testjail ./rootfs /bin/hostname | grep -q '^testjail$'; }
t_pid1()     { $MJ run --name t-pid ./rootfs /bin/sh -c 'echo $$' | grep -qx 1; }
t_os()       { $MJ run --name t-os ./rootfs /bin/cat /etc/os-release | grep -qi alpine; }
t_ps()       { ! $MJ run --name t-ps ./rootfs /bin/ps | grep -q microjail; }

t_namespaces() {
    for ns in ipc pid uts mnt; do
        host=$(readlink /proc/self/ns/$ns)
        inside=$($MJ run --name t-ns ./rootfs /bin/readlink /proc/self/ns/$ns | head -1)
        [ -n "$inside" ] && [ "$inside" != "$host" ] || return 1
    done
}

t_mounts() {
    n=$($MJ run --name t-mnt ./rootfs /bin/cat /proc/self/mountinfo | grep -c ' - ')
    [ "$n" -le 4 ]
}

t_limits() {
    $MJ run --name t-limits --mem 64M --cpu 30 ./rootfs /bin/sleep 4 >/dev/null 2>&1 &
    local p=$!
    sleep 1
    local mem cpu
    mem=$(cat /sys/fs/cgroup/microjail/t-limits/memory.max 2>/dev/null)
    cpu=$(cat /sys/fs/cgroup/microjail/t-limits/cpu.max 2>/dev/null)
    wait $p
    [ "$mem" = "67108864" ] && [ "$cpu" = "30000 100000" ]
}

t_oom() {
    [ -x ./rootfs/usr/local/bin/memhog ] || return 1
    $MJ run --name t-oom --mem 30M ./rootfs /usr/local/bin/memhog | grep -q 'killed by signal 9'
}

t_cleanup() {
    [ ! -d /sys/fs/cgroup/microjail/t-limits ] && [ ! -d /sys/fs/cgroup/microjail/t-oom ] \
        && [ ! -e /var/run/microjail/t-limits.state ] && [ ! -e /var/run/microjail/t-oom.state ]
}

t_lifecycle() {
    $MJ run --name t-life ./rootfs /bin/sleep 60 >/dev/null 2>&1 &
    local p=$!
    sleep 1
    if ! $MJ list | grep -q t-life; then
        $MJ stop t-life >/dev/null 2>&1; wait $p; return 1
    fi
    $MJ stop t-life >/dev/null 2>&1
    wait $p
    ! $MJ list | grep -q t-life
}

t_badmem()   { ! $MJ run --mem abc ./rootfs /bin/sh; }
t_badroot()  { ! $MJ run ./does-not-exist /bin/sh; }
t_dupname()  {
    $MJ run --name t-dup ./rootfs /bin/sleep 5 >/dev/null 2>&1 &
    local p=$!
    sleep 1
    $MJ run --name t-dup ./rootfs /bin/sh 2>&1 | grep -q 'already running'
    local r=$?
    wait $p
    return $r
}

echo "== MicroJail checks =="
check "own hostname"                   t_hostname
check "process is PID 1 in container"  t_pid1
check "container sees Alpine rootfs"   t_os
check "host processes not visible"     t_ps
check "ipc/pid/uts/mnt namespaces differ from host" t_namespaces
check "pivot_root hides host mounts"   t_mounts
check "memory and cpu limits applied"  t_limits
check "memory hog is OOM-killed"       t_oom
check "cgroups and state files cleaned up" t_cleanup
check "list and stop work"             t_lifecycle
check "bad --mem rejected"             t_badmem
check "missing rootfs rejected"        t_badroot
check "duplicate name rejected"        t_dupname

echo "== $PASS passed, $FAIL failed =="
[ "$FAIL" -eq 0 ]
