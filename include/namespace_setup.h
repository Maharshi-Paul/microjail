#pragma once
#include <string>
#include <vector>
#include <sys/types.h>

struct ContainerConfig {
    std::string name, hostname, rootfs;
    std::vector<std::string> cmd;
    long mem_bytes = 0;
    int cpu_percent = 0;
};

pid_t spawn_container(const ContainerConfig& cfg, int sync_fd);
