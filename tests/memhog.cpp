#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

int main() {
    const size_t CHUNK = 10 * 1024 * 1024;
    for (int i = 1; ; i++) {
        char* p = (char*)malloc(CHUNK);
        if (!p) { printf("malloc failed\n"); return 1; }
        memset(p, 1, CHUNK);          // touch the pages so they really count
        printf("allocated %d MB\n", i * 10);
        fflush(stdout);
        usleep(200000);
    }
}
