#include "state.h"
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>

static const std::string DIR_PATH = "/var/run/microjail";

static std::string file_of(const std::string& name) {
    return DIR_PATH + "/" + name + ".state";
}

bool state_save(const ContainerState& st) {
    mkdir(DIR_PATH.c_str(), 0755);
    FILE* f = fopen(file_of(st.name).c_str(), "w");
    if (!f) {
        perror("state_save");
        return false;
    }
    fprintf(f, "name=%s\npid=%d\nstart=%ld\nmem=%ld\ncpu=%d\nrootfs=%s\n",
            st.name.c_str(), (int)st.pid, st.start_time,
            st.mem_bytes, st.cpu_percent, st.rootfs.c_str());
    fclose(f);
    return true;
}

bool state_load(const std::string& name, ContainerState& out) {
    std::ifstream in(file_of(name));
    if (!in) return false;
    std::string line;
    while (std::getline(in, line)) {
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string k = line.substr(0, eq);
        std::string v = line.substr(eq + 1);
        if (k == "name") out.name = v;
        else if (k == "pid") out.pid = atoi(v.c_str());
        else if (k == "start") out.start_time = atol(v.c_str());
        else if (k == "mem") out.mem_bytes = atol(v.c_str());
        else if (k == "cpu") out.cpu_percent = atoi(v.c_str());
        else if (k == "rootfs") out.rootfs = v;
    }
    return !out.name.empty();
}

void state_remove(const std::string& name) {
    unlink(file_of(name).c_str());
}

std::vector<ContainerState> state_list() {
    std::vector<ContainerState> result;
    DIR* d = opendir(DIR_PATH.c_str());
    if (!d) return result;
    const std::string ext = ".state";
    struct dirent* e;
    while ((e = readdir(d)) != nullptr) {
        std::string fn = e->d_name;
        if (fn.size() <= ext.size() ||
            fn.compare(fn.size() - ext.size(), ext.size(), ext) != 0)
            continue;
        ContainerState st;
        if (state_load(fn.substr(0, fn.size() - ext.size()), st))
            result.push_back(st);
    }
    closedir(d);
    return result;
}
