#define _POSIX_C_SOURCE 200809L /* getopt, clock_gettime, nanosleep, sysconf */

#include "proc.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

typedef struct row {
    proc_sample sample;
    double cpu_percent;
    long rss_kib;
} row;

typedef enum { SORT_CPU, SORT_MEM } sort_key;

static void usage(FILE *out, const char *prog)
{
    fprintf(out,
            "Usage: %s [-i SECONDS] [-n COUNT] [-s cpu|mem]\n"
            "\n"
            "Show each process's CPU and resident memory use, read from /proc.\n"
            "\n"
            "  -i SECONDS  how long to measure CPU use for, 0.1 to 60 (default 1)\n"
            "  -n COUNT    show only the first COUNT processes (default: all)\n"
            "  -s KEY      sort by 'cpu' (default) or 'mem'\n"
            "  -p DIR      read from DIR instead of /proc (used by the tests)\n"
            "  -h          show this help\n",
            prog);
}

static int is_pid_name(const char *s)
{
    if (*s == '\0')
        return 0;
    for (; *s != '\0'; s++)
        if (!isdigit((unsigned char)*s))
            return 0;
    return 1;
}

/* Read every process under proc_root. Processes that exit mid-scan are skipped. */
static int take_snapshot(const char *proc_root, proc_sample **out, size_t *count)
{
    DIR *dir = opendir(proc_root);
    if (dir == NULL)
        return -1;

    size_t capacity = 256;
    size_t n = 0;
    proc_sample *samples = malloc(capacity * sizeof *samples);
    if (samples == NULL) {
        closedir(dir);
        return -1;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (!is_pid_name(entry->d_name))
            continue;
        if (n == capacity) {
            capacity *= 2;
            proc_sample *grown = realloc(samples, capacity * sizeof *samples);
            if (grown == NULL) {
                free(samples);
                closedir(dir);
                return -1;
            }
            samples = grown;
        }
        pid_t pid = (pid_t)strtol(entry->d_name, NULL, 10);
        if (proc_read_sample(proc_root, pid, &samples[n]) == 0)
            n++;
    }
    closedir(dir);

    *out = samples;
    *count = n;
    return 0;
}

static int compare_pid(const void *a, const void *b)
{
    const proc_sample *x = a;
    const proc_sample *y = b;
    return (x->pid > y->pid) - (x->pid < y->pid);
}

static int compare_cpu_desc(const void *a, const void *b)
{
    const row *x = a;
    const row *y = b;
    if (x->cpu_percent != y->cpu_percent)
        return x->cpu_percent < y->cpu_percent ? 1 : -1;
    if (x->rss_kib != y->rss_kib)
        return x->rss_kib < y->rss_kib ? 1 : -1;
    return compare_pid(&x->sample, &y->sample);
}

static int compare_mem_desc(const void *a, const void *b)
{
    const row *x = a;
    const row *y = b;
    if (x->rss_kib != y->rss_kib)
        return x->rss_kib < y->rss_kib ? 1 : -1;
    if (x->cpu_percent != y->cpu_percent)
        return x->cpu_percent < y->cpu_percent ? 1 : -1;
    return compare_pid(&x->sample, &y->sample);
}

static void sleep_seconds(double seconds)
{
    struct timespec remaining;
    remaining.tv_sec = (time_t)seconds;
    remaining.tv_nsec = (long)((seconds - (double)remaining.tv_sec) * 1e9);
    while (nanosleep(&remaining, &remaining) == -1 && errno == EINTR)
        continue;
}

static double seconds_between(const struct timespec *start, const struct timespec *finish)
{
    return (double)(finish->tv_sec - start->tv_sec)
         + (double)(finish->tv_nsec - start->tv_nsec) / 1e9;
}

static int parse_interval(const char *text, double *out)
{
    char *text_end;
    errno = 0;
    double value = strtod(text, &text_end);
    /* The negated range check also rejects NaN. */
    if (text_end == text || *text_end != '\0' || errno != 0 || !(value >= 0.1 && value <= 60.0))
        return -1;
    *out = value;
    return 0;
}

static int parse_count(const char *text, long *out)
{
    char *text_end;
    errno = 0;
    long value = strtol(text, &text_end, 10);
    if (text_end == text || *text_end != '\0' || errno != 0 || value < 1)
        return -1;
    *out = value;
    return 0;
}

int main(int argc, char **argv)
{
    const char *prog = argv[0];
    const char *proc_root = "/proc";
    double interval = 1.0;
    long limit = 0; /* 0 means show every process */
    sort_key sort_by = SORT_CPU;
    int opt;

    while ((opt = getopt(argc, argv, "i:n:s:p:h")) != -1) {
        switch (opt) {
        case 'i':
            if (parse_interval(optarg, &interval) != 0) {
                fprintf(stderr, "%s: -i expects seconds from 0.1 to 60, got '%s'\n", prog, optarg);
                return 2;
            }
            break;
        case 'n':
            if (parse_count(optarg, &limit) != 0) {
                fprintf(stderr, "%s: -n expects a positive whole number, got '%s'\n", prog, optarg);
                return 2;
            }
            break;
        case 's':
            if (strcmp(optarg, "cpu") == 0) {
                sort_by = SORT_CPU;
            } else if (strcmp(optarg, "mem") == 0) {
                sort_by = SORT_MEM;
            } else {
                fprintf(stderr, "%s: -s expects 'cpu' or 'mem', got '%s'\n", prog, optarg);
                return 2;
            }
            break;
        case 'p':
            proc_root = optarg;
            break;
        case 'h':
            usage(stdout, prog);
            return 0;
        default:
            usage(stderr, prog);
            return 2;
        }
    }
    if (optind < argc) {
        fprintf(stderr, "%s: unexpected argument '%s'\n", prog, argv[optind]);
        usage(stderr, prog);
        return 2;
    }

    long ticks_per_second = sysconf(_SC_CLK_TCK);
    long page_size = sysconf(_SC_PAGESIZE);
    if (ticks_per_second <= 0 || page_size <= 0) {
        fprintf(stderr, "%s: cannot read the clock tick rate or page size\n", prog);
        return 1;
    }

    proc_sample *before = NULL;
    proc_sample *after = NULL;
    size_t n_before = 0;
    size_t n_after = 0;
    struct timespec start;
    struct timespec finish;

    if (take_snapshot(proc_root, &before, &n_before) != 0) {
        fprintf(stderr, "%s: cannot read %s: %s\n", prog, proc_root, strerror(errno));
        return 1;
    }
    clock_gettime(CLOCK_MONOTONIC, &start);
    sleep_seconds(interval);
    if (take_snapshot(proc_root, &after, &n_after) != 0) {
        fprintf(stderr, "%s: cannot read %s: %s\n", prog, proc_root, strerror(errno));
        free(before);
        return 1;
    }
    clock_gettime(CLOCK_MONOTONIC, &finish);
    double elapsed = seconds_between(&start, &finish);

    /* CPU% needs two readings of the same process, so index the first one by PID. */
    qsort(before, n_before, sizeof *before, compare_pid);

    row *rows = malloc((n_after > 0 ? n_after : 1) * sizeof *rows);
    if (rows == NULL) {
        fprintf(stderr, "%s: out of memory\n", prog);
        free(after);
        free(before);
        return 1;
    }
    for (size_t i = 0; i < n_after; i++) {
        const proc_sample *earlier =
            bsearch(&after[i], before, n_before, sizeof *before, compare_pid);
        rows[i].sample = after[i];
        rows[i].rss_kib = proc_rss_kib(after[i].rss_pages, page_size);
        rows[i].cpu_percent =
            earlier == NULL ? 0.0
                            : proc_cpu_percent(earlier->cpu_ticks, after[i].cpu_ticks,
                                               ticks_per_second, elapsed);
    }
    qsort(rows, n_after, sizeof *rows, sort_by == SORT_MEM ? compare_mem_desc : compare_cpu_desc);

    size_t shown = n_after;
    if (limit > 0 && (size_t)limit < shown)
        shown = (size_t)limit;

    printf("%7s %6s %10s %s %s\n", "PID", "CPU%", "RSS(KiB)", "S", "NAME");
    for (size_t i = 0; i < shown; i++)
        printf("%7d %6.1f %10ld %c %s\n", (int)rows[i].sample.pid, rows[i].cpu_percent,
               rows[i].rss_kib, rows[i].sample.state, rows[i].sample.name);

    free(rows);
    free(after);
    free(before);
    return 0;
}
