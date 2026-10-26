#include <cstdio>
#include <cstdlib>
#include <csignal>
#include <cerrno>
#include <ctime>
#include <cctype>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string>
#include <vector>
#include "namespace_setup.h"
#include "cgroup_manager.h"
#include "logger.h"
#include "state.h"

static void usage() {
    fprintf(stderr,
        "usage:\n"
        "  microjail run [--name N] [--mem 50M] [--cpu 50] [--hostname H] <rootfs> <cmd> [args...]\n"
        "  microjail list\n"
        "  microjail stop <name>\n");
}

// "100M" -> bytes. Returns -1 on bad input.
static long parse_size(const std::string& s) {
    char* end = nullptr;
    long n = strtol(s.c_str(), &end, 10);
    if (end == s.c_str() || n <= 0) return -1;
    char suffix = *end;
    if (suffix != '\0' && end[1] != '\0') return -1;
    switch (suffix) {
        case '\0': return n;
        case 'K': case 'k': return n * 1024;
        case 'M': case 'm': return n * 1024 * 1024;
        case 'G': case 'g': return n * 1024L * 1024 * 1024;
        default:  return -1;
    }
}

// the name ends up inside file paths, so allow only safe characters
static bool valid_name(const std::string& s) {
    if (s.empty() || s.size() > 32) return false;
    for (char c : s)
        if (!isalnum((unsigned char)c) && c != '-' && c != '_') return false;
    return true;
}

static int cmd_run(int argc, char** argv) {
    ContainerConfig cfg;
    cfg.name = "demo";
    cfg.hostname = "jail";
    cfg.mem_bytes = 50L * 1024 * 1024;
    cfg.cpu_percent = 50;

    // options come first; the first word without "--" is the rootfs
    int i = 2;
    for (; i < argc; i++) {
        std::string a = argv[i];
        if (a.rfind("--", 0) != 0) break;
        if (i + 1 >= argc) {
            fprintf(stderr, "missing value for %s\n", a.c_str());
            return 1;
        }
        std::string v = argv[++i];
        if (a == "--name") {
            cfg.name = v;
        } else if (a == "--hostname") {
            cfg.hostname = v;
        } else if (a == "--mem") {
            cfg.mem_bytes = parse_size(v);
            if (cfg.mem_bytes < 0) {
                fprintf(stderr, "bad --mem value: %s (try 50M)\n", v.c_str());
                return 1;
            }
        } else if (a == "--cpu") {
            cfg.cpu_percent = atoi(v.c_str());
            if (cfg.cpu_percent < 1 || cfg.cpu_percent > 100) {
                fprintf(stderr, "--cpu must be 1-100\n");
                return 1;
            }
        } else {
            fprintf(stderr, "unknown option: %s\n", a.c_str());
            return 1;
        }
    }
    if (argc - i < 2) {          // need a rootfs and at least one command word
        usage();
        return 1;
    }
    cfg.rootfs = argv[i];
    for (int j = i + 1; j < argc; j++) cfg.cmd.push_back(argv[j]);

    if (!valid_name(cfg.name)) {
        fprintf(stderr, "bad --name: use letters, digits, '-' and '_' only\n");
        return 1;
    }
    ContainerState old;
    if (state_load(cfg.name, old)) {
        if (kill(old.pid, 0) == 0) {
            fprintf(stderr, "container '%s' is already running (pid %d)\n",
                    cfg.name.c_str(), (int)old.pid);
            return 1;
        }
        state_remove(cfg.name);      // stale file left by a crash
        cgroup_remove(cfg.name);
    }

    int fds[2];
    if (pipe2(fds, O_CLOEXEC) != 0) {
        perror("pipe2");
        return 1;
    }

    pid_t pid = spawn_container(cfg, fds[0]);
    if (pid < 0) {
        perror("clone");
        return 1;
    }
    close(fds[0]);

    std::string id = cfg.name;
    if (!cgroup_create(id) ||
        !cgroup_set_memory(id, cfg.mem_bytes) ||
        !cgroup_set_cpu(id, cfg.cpu_percent) ||
        !cgroup_add_pid(id, pid)) {
        fprintf(stderr, "cgroup setup failed\n");
        log_event("ERROR", id + " cgroup setup failed");
        close(fds[1]);
        kill(pid, SIGKILL);
        waitpid(pid, nullptr, 0);
        cgroup_remove(id);
        return 1;
    }

    log_event("START", id + " pid=" + std::to_string(pid) +
              " mem=" + std::to_string(cfg.mem_bytes / (1024 * 1024)) + "M" +
              " cpu=" + std::to_string(cfg.cpu_percent) + "%" +
              " rootfs=" + cfg.rootfs);

    ContainerState st;
    st.name = id;
    st.pid = pid;
    st.start_time = time(nullptr);
    st.mem_bytes = cfg.mem_bytes;
    st.cpu_percent = cfg.cpu_percent;
    st.rootfs = cfg.rootfs;
    state_save(st);

    // green light: the child is inside its cgroup, let it run
    if (write(fds[1], "x", 1) != 1) perror("write");
    close(fds[1]);

    int status = 0;
    waitpid(pid, &status, 0);
    cgroup_remove(id);
    state_remove(id);

    if (WIFEXITED(status)) {
        printf("container '%s' exited with code %d\n", id.c_str(), WEXITSTATUS(status));
        log_event("EXIT", id + " code=" + std::to_string(WEXITSTATUS(status)));
    } else if (WIFSIGNALED(status)) {
        printf("container '%s' killed by signal %d\n", id.c_str(), WTERMSIG(status));
        log_event("EXIT", id + " killed by signal " + std::to_string(WTERMSIG(status)));
    }
    return 0;
}

static int cmd_list() {
    std::vector<ContainerState> all = state_list();
    printf("%-12s %-8s %-8s %-6s %-9s %s\n",
           "NAME", "PID", "MEM", "CPU%", "STARTED", "ROOTFS");
    int shown = 0;
    for (const ContainerState& st : all) {
        if (kill(st.pid, 0) != 0 && errno == ESRCH) {   // stale file
            state_remove(st.name);
            continue;
        }
        char ts[16];
        time_t t = st.start_time;
        struct tm tmv;
        localtime_r(&t, &tmv);
        strftime(ts, sizeof(ts), "%H:%M:%S", &tmv);
        std::string mem = std::to_string(st.mem_bytes / (1024 * 1024)) + "M";
        printf("%-12s %-8d %-8s %-6d %-9s %s\n", st.name.c_str(), (int)st.pid,
               mem.c_str(), st.cpu_percent, ts, st.rootfs.c_str());
        shown++;
    }
    if (shown == 0) printf("(no running containers)\n");
    return 0;
}

static int cmd_stop(const std::string& name) {
    ContainerState st;
    if (!state_load(name, st)) {
        fprintf(stderr, "no such container: %s\n", name.c_str());
        return 1;
    }
    if (kill(st.pid, 0) != 0) {
        fprintf(stderr, "'%s' is not running; cleaning up\n", name.c_str());
        state_remove(name);
        cgroup_remove(name);
        return 1;
    }
    printf("stopping '%s' (pid %d)...\n", name.c_str(), (int)st.pid);
    kill(st.pid, SIGTERM);
    for (int i = 0; i < 30; i++) {          // up to 3 seconds
        usleep(100000);
        if (kill(st.pid, 0) != 0) {
            printf("stopped\n");
            return 0;
        }
    }
    printf("did not stop in time, sending SIGKILL\n");
    kill(st.pid, SIGKILL);
    usleep(200000);
    printf("stopped\n");
    return 0;
}

int main(int argc, char** argv) {
    if (geteuid() != 0) {
        fprintf(stderr, "microjail must run as root (use sudo)\n");
        return 1;
    }
    if (argc < 2) {
        usage();
        return 1;
    }
    std::string c = argv[1];
    if (c == "run")  return cmd_run(argc, argv);
    if (c == "list") return cmd_list();
        if (c == "stop") {
        if (argc < 3) { usage(); return 1; }
        return cmd_stop(argv[2]);
    }
    usage();
    return 1;
}
