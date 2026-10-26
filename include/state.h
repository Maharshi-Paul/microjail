#pragma once
#include <string>
#include <vector>
#include <sys/types.h>

struct ContainerState {
    std::string name, rootfs;
    pid_t pid = 0;
    long start_time = 0;
    long mem_bytes = 0;
    int cpu_percent = 0;
};

bool state_save(const ContainerState& st);
bool state_load(const std::string& name, ContainerState& out);
void state_remove(const std::string& name);
std::vector<ContainerState> state_list();
