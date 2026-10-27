#include "namespace_setup.h"
#include <sched.h>
#include <csignal>
#include <unistd.h>
#include <cstdio>
#include <string>
#include <vector>
#include <sys/mount.h>

struct ChildArgs {
    const ContainerConfig* cfg;
    int sync_fd;
    int close_fd;
};

static int child_main(void* arg) {
    ChildArgs* a = static_cast<ChildArgs*>(arg);
    const ContainerConfig& cfg = *a->cfg;
    
    // wait for the parent's "go" signal
    if (a->close_fd >= 0) close(a->close_fd);   // drop our copy of the write end
    if (a->sync_fd >= 0) {
        char c;
        if (read(a->sync_fd, &c, 1) != 1) {
            fprintf(stderr, "container aborted: parent went away before start\n");
            return 1;
        }
    }

    if (sethostname(cfg.hostname.c_str(), cfg.hostname.size()) != 0) {
        perror("sethostname");
        return 1;
    }

    // stop our mounts from leaking out to the host
    if (mount(nullptr, "/", nullptr, MS_REC | MS_PRIVATE, nullptr) != 0) {
        perror("mount private");
        return 1;
    }

    // the Alpine folder becomes "/"
    if (chroot(cfg.rootfs.c_str()) != 0) {
        perror("chroot");
        return 1;
    }
    if (chdir("/") != 0) {
        perror("chdir");
        return 1;
    }

    // fresh /proc for this container
    if (mount("proc", "/proc", "proc", 0, nullptr) != 0) {
        perror("mount proc");
        return 1;
    }

    std::vector<char*> argv;
    for (const std::string& s : cfg.cmd)
        argv.push_back(const_cast<char*>(s.c_str()));
    argv.push_back(nullptr);

    execvp(argv[0], argv.data());
    perror("execvp");
    return 1;
}

pid_t spawn_container(const ContainerConfig& cfg, int sync_fd, int close_fd) {
    const int STACK_SIZE = 1024 * 1024;
    char* stack = new char[STACK_SIZE];

    ChildArgs args{&cfg, sync_fd, close_fd};

    pid_t pid = clone(child_main, stack + STACK_SIZE,
                      CLONE_NEWPID | CLONE_NEWUTS | CLONE_NEWNS | CLONE_NEWIPC | SIGCHLD, &args);

    delete[] stack;
    return pid;
}
