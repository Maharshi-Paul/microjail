#include "fs_isolation.h"
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstdlib>

// swap the root mount: the new root becomes "/", the old one is unmounted
static bool try_pivot_root(const char* root) {
    // new root must be a mount point, so bind-mount it onto itself
    if (mount(root, root, nullptr, MS_BIND | MS_REC, nullptr) != 0) {
        perror("bind mount rootfs");
        return false;
    }
    if (chdir(root) != 0) {
        perror("chdir rootfs");
        return false;
    }
    if (mkdir(".pivot_old", 0700) != 0 && errno != EEXIST) {
        perror("mkdir .pivot_old");
        return false;
    }
    if (syscall(SYS_pivot_root, ".", ".pivot_old") != 0) {
        perror("pivot_root");
        return false;
    }
    if (chdir("/") != 0) {
        perror("chdir /");
        return false;
    }
    if (umount2("/.pivot_old", MNT_DETACH) != 0) {
        perror("umount old root");
        return false;
    }
    rmdir("/.pivot_old");
    return true;
}

bool isolate_rootfs(const std::string& rootfs) {
    char abs[PATH_MAX];
    if (!realpath(rootfs.c_str(), abs)) {
        perror("realpath rootfs");
        return false;
    }
    if (try_pivot_root(abs)) return true;

    // fallback: plain chroot (weaker, but still works)
    fprintf(stderr, "warning: pivot_root failed, falling back to chroot\n");
    if (chroot(abs) != 0) {
        perror("chroot");
        return false;
    }
    if (chdir("/") != 0) {
        perror("chdir");
        return false;
    }
    return true;
}
