#include "namespace_setup.h"
#include <sched.h>
#include <csignal>
#include <unistd.h>
#include <cstdio>
#include <vector>
#include <string>

struct ChildArgs {
    const ContainerConfig* cfg;
    int sync_fd;
};

static int child_main(void* arg) {
    ChildArgs* a = static_cast<ChildArgs*>(arg);
    const ContainerConfig& cfg = *a->cfg;

    if (sethostname(cfg.hostname.c_str(), cfg.hostname.size()) != 0) {
        perror("sethostname");
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

pid_t spawn_container(const ContainerConfig& cfg, int sync_fd) {
    const int STACK_SIZE = 1024 * 1024;
    char* stack = new char[STACK_SIZE];

    ChildArgs args{&cfg, sync_fd};

    pid_t pid = clone(child_main, stack + STACK_SIZE,
                      CLONE_NEWPID | CLONE_NEWUTS | SIGCHLD, &args);

    delete[] stack;
    return pid;
}
