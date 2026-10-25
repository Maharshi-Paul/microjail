#pragma once
#include <string>
#include <sys/types.h>

int wait_and_monitor(pid_t pid, const std::string& id);
