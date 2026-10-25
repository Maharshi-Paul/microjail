#pragma once
#include <string>

bool isolate_rootfs(const std::string& rootfs);  // chroot now, pivot_root later
