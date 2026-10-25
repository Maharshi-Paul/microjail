#include <cstdio>
#include <sys/wait.h>
#include "namespace_setup.h"

int main() {
    ContainerConfig cfg;
    cfg.name = "demo";
    cfg.hostname = "jail";
    cfg.rootfs = "./rootfs";
    cfg.cmd = {"/bin/sh"};

    pid_t pid = spawn_container(cfg, -1);
    if (pid < 0) {
        perror("clone");
        return 1;
    }
    printf("parent: child's pid as seen from outside is %d\n", pid);

    int status = 0;
    waitpid(pid, &status, 0);
    printf("parent: child finished\n");
    return 0;
}
