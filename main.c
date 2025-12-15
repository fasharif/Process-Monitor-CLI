#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <unistd.h>

typedef struct {
    int pid;
    char name[256];
    double cpu;
    long memory;
} ProcInfo;

static int is_number(const char *s) {
    for (; *s; s++)
        if (*s < '0' || *s > '9') return 0;
    return 1;
}

static void read_process_info(int pid) {
    char path[256], line[512], name[256] = "?", state;
    long rss = 0;
    double utime = 0, stime = 0, total_time = 0;
    double uptime = 0, seconds = 0, cpu_usage = 0;
    FILE *file;

    sprintf(path, "/proc/%d/stat", pid);
    file = fopen(path, "r");
    if (!file) return;

    fscanf(file, "%d (%255[^)]) %c", &pid, name, &state);
    for (int i = 0; i < 10; i++) fscanf(file, "%*s");
    fscanf(file, "%lf %lf", &utime, &stime);
    for (int i = 0; i < 7; i++) fscanf(file, "%*s");
    fscanf(file, "%ld", &rss);
    fclose(file);

    file = fopen("/proc/uptime", "r");
    if (file) { fscanf(file, "%lf", &uptime); fclose(file); }

    total_time = utime + stime;
    long hertz = sysconf(_SC_CLK_TCK);
    seconds = uptime - (total_time / hertz);
    if (seconds > 0)
        cpu_usage = 100.0 * ((total_time / hertz) / seconds);

    printf("%-6d %-25s %6.2f%% %8ld KB\n", pid, name, cpu_usage, rss * 4);
}

int main(void) {
    DIR *proc = opendir("/proc");
    struct dirent *entry;

    if (!proc) {
        perror("opendir");
        return 1;
    }

    printf("%-6s %-25s %-8s %-10s\n", "PID", "NAME", "CPU", "MEMORY");
    printf("--------------------------------------------------------------\n");

    while ((entry = readdir(proc)) != NULL) {
        if (is_number(entry->d_name))
            read_process_info(atoi(entry->d_name));
    }

    closedir(proc);
    return 0;
}
