#include <csignal>
#include <string>
#include "cgroup_manager.h"
#include <cstdio>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>
#include "namespace_setup.h"

int main() {
    ContainerConfig cfg;
    cfg.name = "demo";
    cfg.hostname = "jail";
    cfg.rootfs = "./rootfs";
    cfg.cmd = {"/bin/sh"};
    cfg.mem_bytes = 50L * 1024 * 1024;   // 50 MB
    cfg.cpu_percent = 50;                // half of one CPU

    int fds[2];
    if (pipe2(fds, O_CLOEXEC) != 0) {
        perror("pipe2");
        return 1;
    }

    pid_t pid = spawn_container(cfg, fds[0]);
    if (pid < 0) {
        perror("clone");
        return 1;
    }
    close(fds[0]);

    std::string id = cfg.name;
    if (!cgroup_create(id) ||
        !cgroup_set_memory(id, cfg.mem_bytes) ||
        !cgroup_set_cpu(id, cfg.cpu_percent) ||
        !cgroup_add_pid(id, pid)) {
        fprintf(stderr, "cgroup setup failed\n");
        kill(pid, SIGKILL);
        waitpid(pid, nullptr, 0);
        cgroup_remove(id);
        return 1;
    }

    
    if (write(fds[1], "x", 1) != 1) perror("write");
    close(fds[1]);

    int status = 0;
    waitpid(pid, &status, 0);
    cgroup_remove(id);
    printf("container exited\n");
    return 0;
}
