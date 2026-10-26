#include "process_monitor.h"
#include "logger.h"
#include <sys/wait.h>
#include <unistd.h>
#include <csignal>
#include <cerrno>
#include <cstdio>
#include <fstream>
#include <string>

static const std::string CG = "/sys/fs/cgroup/microjail/";

static volatile sig_atomic_t g_signal = 0;
static void on_signal(int sig) { g_signal = sig; }

// read a file that holds one number
static long read_long(const std::string& path) {
    std::ifstream f(path);
    long v = -1;
    if (f >> v) return v;
    return -1;
}

// read "key value" lines (memory.events, cpu.stat)
static long read_key(const std::string& path, const std::string& key) {
    std::ifstream f(path);
    std::string k;
    long v;
    while (f >> k >> v) {
        if (k == key) return v;
    }
    return -1;
}

int wait_and_monitor(pid_t pid, const std::string& id) {
    // Ctrl+C or "kill" on microjail itself: stop the container, then clean up
    struct sigaction sa{};
    sa.sa_handler = on_signal;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);

    int status = 0;
    int tick = 0;
    while (true) {
        pid_t r = waitpid(pid, &status, WNOHANG);
        if (r == pid) break;
        if (r < 0 && errno != EINTR) {
            perror("waitpid");
            break;
        }
        if (g_signal) {
            log_event("INFO", id + " got signal " + std::to_string((int)g_signal) +
                      ", killing container");
            kill(pid, SIGKILL);
            g_signal = 0;
        }
        if (tick % 25 == 0) {                         // every ~5 seconds
            long mem = read_long(CG + id + "/memory.current");
            long cpu = read_key(CG + id + "/cpu.stat", "usage_usec");
            log_event("USAGE", id + " mem=" + std::to_string(mem / 1024) +
                      "KB cpu_usec=" + std::to_string(cpu));
        }
        tick++;
        usleep(200000);
    }

    long peak = read_long(CG + id + "/memory.peak");
    long oom = read_key(CG + id + "/memory.events", "oom_kill");
    log_event("USAGE", id + " peak_mem=" + std::to_string(peak / 1024) + "KB");
    if (oom > 0)
        log_event("OOM", id + " killed by memory limit, oom_kill=" + std::to_string(oom));
    return status;
}
