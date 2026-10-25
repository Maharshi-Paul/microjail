#include "cgroup_manager.h"
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <string>

static const std::string ROOT = "/sys/fs/cgroup/microjail";

// write a value into a cgroup control file
static bool write_file(const std::string& path, const std::string& value,
                       bool verbose = true) {
    int fd = open(path.c_str(), O_WRONLY);
    if (fd < 0) {
        if (verbose) fprintf(stderr, "open %s: %s\n", path.c_str(), strerror(errno));
        return false;
    }
    ssize_t n = write(fd, value.c_str(), value.size());
    int saved = errno;
    close(fd);
    if (n != (ssize_t)value.size()) {
        if (verbose) fprintf(stderr, "write %s: %s\n", path.c_str(), strerror(saved));
        return false;
    }
    return true;
}

bool cgroup_create(const std::string& id) {
    // parent group "microjail"
    if (mkdir(ROOT.c_str(), 0755) != 0 && errno != EEXIST) {
        perror("mkdir microjail cgroup");
        return false;
    }
    // allow cpu + memory limits in child groups
    write_file("/sys/fs/cgroup/cgroup.subtree_control", "+cpu +memory");
    write_file(ROOT + "/cgroup.subtree_control", "+cpu +memory");

    // this container's own group
    std::string dir = ROOT + "/" + id;
    if (mkdir(dir.c_str(), 0755) != 0 && errno != EEXIST) {
        perror("mkdir container cgroup");
        return false;
    }
    return true;
}

bool cgroup_set_memory(const std::string& id, long bytes) {
    std::string dir = ROOT + "/" + id;
    // no swap, so the limit is a hard wall
    write_file(dir + "/memory.swap.max", "0", false);
    return write_file(dir + "/memory.max", std::to_string(bytes));
}

bool cgroup_set_cpu(const std::string& id, int percent) {
    // cpu.max = "quota period" in microseconds. 50% -> "50000 100000"
    long quota = (long)percent * 1000;
    std::string value = std::to_string(quota) + " 100000";
    return write_file(ROOT + "/" + id + "/cpu.max", value);
}

bool cgroup_add_pid(const std::string& id, pid_t pid) {
    return write_file(ROOT + "/" + id + "/cgroup.procs", std::to_string(pid));
}

void cgroup_remove(const std::string& id) {
    std::string dir = ROOT + "/" + id;
    if (rmdir(dir.c_str()) != 0 && errno != ENOENT)
        perror("rmdir cgroup");
}
