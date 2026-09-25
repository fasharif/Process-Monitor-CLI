#define _POSIX_C_SOURCE 200809L

#include "proc.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Field numbers from proc(5). Field 1 is the PID and field 2 the name in parentheses. */
enum {
    FIELD_STATE = 3,
    FIELD_UTIME = 14,
    FIELD_STIME = 15,
    FIELD_RSS = 24, /* resident pages; field 23 is the virtual size in bytes */
};

int proc_parse_stat(const char *line, proc_sample *out)
{
    /* A name can contain spaces and parentheses, e.g. "(my (odd) proc)",
       so it runs from the first '(' to the last ')'. */
    const char *name_start = strchr(line, '(');
    const char *name_end = strrchr(line, ')');
    if (name_start == NULL || name_end == NULL || name_end < name_start)
        return -1;

    char *pid_end;
    errno = 0;
    long pid = strtol(line, &pid_end, 10);
    if (pid_end == line || errno != 0 || pid <= 0 || pid > INT_MAX)
        return -1;

    size_t name_len = (size_t)(name_end - name_start - 1);
    if (name_len >= sizeof out->name)
        name_len = sizeof out->name - 1;
    memcpy(out->name, name_start + 1, name_len);
    out->name[name_len] = '\0';

    char state = '\0';
    unsigned long long utime = 0;
    unsigned long long stime = 0;
    long long rss = -1;
    const char *p = name_end + 1;

    for (int field = FIELD_STATE; field <= FIELD_RSS; field++) {
        while (*p == ' ')
            p++;
        if (*p == '\0' || *p == '\n')
            return -1; /* the line ended before field 24 */
        switch (field) {
        case FIELD_STATE:
            state = *p;
            break;
        case FIELD_UTIME:
            utime = strtoull(p, NULL, 10);
            break;
        case FIELD_STIME:
            stime = strtoull(p, NULL, 10);
            break;
        case FIELD_RSS:
            rss = strtoll(p, NULL, 10);
            break;
        default:
            break;
        }
        while (*p != ' ' && *p != '\0' && *p != '\n')
            p++;
    }
    if (rss < 0)
        return -1;

    out->pid = (pid_t)pid;
    out->state = state;
    out->cpu_ticks = utime + stime;
    out->rss_pages = (long)rss;
    return 0;
}

int proc_read_sample(const char *proc_root, pid_t pid, proc_sample *out)
{
    char path[4096];
    char line[1024];

    int written = snprintf(path, sizeof path, "%s/%d/stat", proc_root, (int)pid);
    if (written < 0 || (size_t)written >= sizeof path)
        return -1;

    FILE *file = fopen(path, "r");
    if (file == NULL)
        return -1; /* the process may have exited since the directory was listed */
    char *read_ok = fgets(line, (int)sizeof line, file);
    fclose(file);
    if (read_ok == NULL)
        return -1;
    return proc_parse_stat(line, out);
}

double proc_cpu_percent(unsigned long long ticks_before, unsigned long long ticks_after,
                        long ticks_per_second, double elapsed_seconds)
{
    /* A smaller second reading means the PID now belongs to a new process. */
    if (ticks_after < ticks_before || ticks_per_second <= 0 || elapsed_seconds <= 0.0)
        return 0.0;
    double cpu_seconds = (double)(ticks_after - ticks_before) / (double)ticks_per_second;
    return 100.0 * cpu_seconds / elapsed_seconds;
}

long proc_rss_kib(long rss_pages, long page_size_bytes)
{
    if (rss_pages <= 0 || page_size_bytes <= 0)
        return 0;
    return rss_pages * (page_size_bytes / 1024);
}
