#ifndef PROC_H
#define PROC_H

#include <sys/types.h>

#define PROC_NAME_MAX 256

/* One reading of a process, taken from /proc/[pid]/stat. */
typedef struct proc_sample {
    pid_t pid;
    char name[PROC_NAME_MAX];
    char state;
    unsigned long long cpu_ticks; /* utime + stime, in clock ticks */
    long rss_pages;               /* resident set size, in pages */
} proc_sample;

/* Parse one line of /proc/[pid]/stat. Returns 0 on success, -1 if the line is malformed. */
int proc_parse_stat(const char *line, proc_sample *out);

/* Read and parse <proc_root>/<pid>/stat. Returns 0 on success, -1 otherwise. */
int proc_read_sample(const char *proc_root, pid_t pid, proc_sample *out);

/* CPU used between two readings, as a percentage of one CPU (like top's default mode). */
double proc_cpu_percent(unsigned long long ticks_before, unsigned long long ticks_after,
                        long ticks_per_second, double elapsed_seconds);

/* Resident memory in KiB, from a page count and the page size in bytes. */
long proc_rss_kib(long rss_pages, long page_size_bytes);

#endif
