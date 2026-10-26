#include "logger.h"
#include <cstdio>
#include <ctime>

static const char* LOG_PATH = "logs/microjail.log";

void log_event(const char* level, const std::string& msg) {
    FILE* f = fopen(LOG_PATH, "a");
    if (!f) return;
    time_t now = time(nullptr);
    struct tm tmv;
    localtime_r(&now, &tmv);
    char ts[32];
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tmv);
    fprintf(f, "%s [%s] %s\n", ts, level, msg.c_str());
    fclose(f);
}
