#pragma once
#include <string>
#include <sys/types.h>

bool cgroup_create(const std::string& id);
bool cgroup_set_memory(const std::string& id, long bytes);
bool cgroup_set_cpu(const std::string& id, int percent);
bool cgroup_add_pid(const std::string& id, pid_t pid);
void cgroup_remove(const std::string& id);
