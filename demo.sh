#!/bin/bash
# MicroJail one-shot demo.
#   sudo ./demo.sh          step by step, press Enter to move on
#   sudo ./demo.sh -a       automatic, 6 second pause between steps
#   sudo ./demo.sh 4        start from step 4 (can be combined with -a)
# Put this file in the repo root (or in scripts/).

if [ "$EUID" -ne 0 ]; then exec sudo "$0" "$@"; fi
cd "$(dirname "$(readlink -f "$0")")" || exit 1
[ -f scripts/fetch_rootfs.sh ] || cd ..
[ -f scripts/fetch_rootfs.sh ] || { echo "Run this from the microjail repo (root or scripts/)."; exit 1; }

AUTO=0; START=1
for a in "$@"; do
  case "$a" in
    -a) AUTO=1 ;;
    [0-9]*) START=$a ;;
  esac
done

B=$'\e[1m'; C=$'\e[36m'; G=$'\e[32m'; Y=$'\e[33m'; N=$'\e[0m'
OUT="$(getent passwd "${SUDO_USER:-root}" | cut -d: -f6)/test_output.txt"

pause() {
  if [ "$AUTO" = 1 ]; then sleep 6
  else read -rp "${G}[Enter] to continue...${N} " _ </dev/tty; fi
}
banner() { clear; echo "${B}${C}=== $* ===${N}"; echo; }
run() { echo "${B}\$ $*${N}"; eval "$*"; echo; }

stop_all() {
  for n in $(./microjail list 2>/dev/null | awk 'NR>1 && $1!="(no" && NF>=3 {print $1}'); do
    ./microjail stop "$n" >/dev/null 2>&1
  done
}
trap 'stop_all; exit 130' INT

preflight() {
  banner "PRE-FLIGHT"
  [ "$(stat -fc %T /sys/fs/cgroup)" = cgroup2fs ] || { echo "cgroups v2 is not active"; exit 1; }
  if [ ! -x ./microjail ]; then
    echo "Building..."
    if [ -f Makefile ]; then make || exit 1
    else g++ -std=c++17 -Wall -Wextra -Iinclude src/*.cpp -o microjail || exit 1; fi
  fi
  [ -d rootfs/bin ] || ./scripts/fetch_rootfs.sh
  [ -x rootfs/usr/local/bin/memhog ] && [ -x rootfs/usr/local/bin/cpuhog ] || ./scripts/install_test_programs.sh
  mkdir -p logs
  stop_all
  run 'stat -fc %T /sys/fs/cgroup'
  run 'ls -l microjail'
  run 'ls rootfs/usr/local/bin'
  run './microjail list'
}

step1() {
  banner "STEP 1: ISOLATION (host vs container)"
  echo "${Y}-- HOST --${N}"
  run 'hostname'
  run 'ls -l /proc/self/ns'
  pause
  banner "STEP 1: ISOLATION (inside the container)"
  CMDS=('hostname' 'echo $$' 'cat /etc/os-release | head -2' 'ls /' 'ps' 'ls -l /proc/self/ns' 'cat /proc/self/mountinfo')
  SCRIPT=""
  for c in "${CMDS[@]}"; do SCRIPT+="echo '\$ $c'"$'\n'"$c"$'\n'; done
  echo "${B}\$ sudo ./microjail run --name box1 --hostname mybox ./rootfs /bin/sh${N}"
  echo "${Y}(the commands below are typed into the container's shell for you)${N}"
  ./microjail run --name box1 --hostname mybox ./rootfs /bin/sh -c "$SCRIPT"
  echo
  pause
  banner "STEP 1: HOST vs CONTAINER NAMESPACE IDs"
  INNER=$(./microjail run --name box1 --hostname mybox ./rootfs /bin/sh -c \
    'for n in cgroup ipc mnt net pid time user uts; do echo "NS $n $(readlink /proc/self/ns/$n)"; done' \
    2>/dev/null | grep '^NS ')
  printf "${B}%-8s %-24s %-24s %s${N}\n" NAMESPACE HOST CONTAINER RESULT
  while read -r _ n id; do
    h=$(readlink "/proc/self/ns/$n")
    if [ "$h" = "$id" ]; then r="SAME (shared with host)"; else r="DIFFERENT (isolated)"; fi
    printf "%-8s %-24s %-24s %s\n" "$n" "$h" "$id" "$r"
  done <<< "$INNER"
  echo
}

step2() {
  banner "STEP 2: LIST AND STOP"
  echo "${Y}(container box1 started in the background)${N}"
  ./microjail run --name box1 ./rootfs /bin/sleep 300 > /tmp/box1.out 2>&1 &
  sleep 2
  run './microjail list'
  run './microjail stop box1'
  wait
  echo "${Y}-- output of the container's terminal --${N}"
  cat /tmp/box1.out; echo
  run './microjail list'
}

step3() {
  banner "STEP 3: MEMORY LIMIT (50M)"
  run './microjail run --name hog --mem 50M ./rootfs /usr/local/bin/memhog'
  run 'tail -5 logs/microjail.log'
}

step4() {
  banner "STEP 4: CPU LIMIT (50%)"
  echo "${Y}(cpuhog started in the background, measuring...)${N}"
  ./microjail run --name cpu --cpu 50 ./rootfs /usr/local/bin/cpuhog > /tmp/cpu.out 2>&1 &
  sleep 4
  run "top -b -n 2 -d 2 | grep -E 'PID USER|cpuhog' | tail -2"
  [ -f /sys/fs/cgroup/microjail/cpu/cpu.max ] && run 'cat /sys/fs/cgroup/microjail/cpu/cpu.max'
  run './microjail list'
  run './microjail stop cpu'
  wait
}

step5() {
  banner "STEP 5: AUTOMATED TESTS"
  stop_all
  run "tests/test_namespaces.sh 2>&1 | tee $OUT"
}

step6() {
  banner "STEP 6: ERROR HANDLING"
  run './microjail run --mem abc ./rootfs /bin/sh'
  run './microjail run ./nope /bin/sh'
  echo "${Y}(container dup started in the background)${N}"
  ./microjail run --name dup ./rootfs /bin/sleep 300 > /dev/null 2>&1 &
  sleep 2
  run './microjail run --name dup ./rootfs /bin/sh'
  run './microjail stop dup'
  wait
}

step7() {
  banner "STEP 7: VALGRIND"
  command -v valgrind >/dev/null || { echo "valgrind missing: sudo apt install -y valgrind"; return; }
  run 'valgrind --leak-check=full ./microjail list 2>&1 | tail -14'
}

preflight
[ "$START" -le 1 ] && pause
for i in 1 2 3 4 5 6 7; do
  if [ "$i" -ge "$START" ]; then "step$i"; pause; fi
done
stop_all
banner "DEMO COMPLETE"
