#include "proc.h"

#include <stdio.h>
#include <string.h>

static int checks;
static int failures;

#define CHECK(condition)                                                                   \
    do {                                                                                   \
        checks++;                                                                          \
        if (!(condition)) {                                                                \
            failures++;                                                                    \
            fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #condition); \
        }                                                                                  \
    } while (0)

static int close_to(double actual, double expected)
{
    double difference = actual - expected;
    return difference < 1e-9 && difference > -1e-9;
}

/* Captured from /proc/1/stat of a shell in a Linux container. Field 23 (vsize) is
   1691648 bytes and field 24 (rss) is 184 pages; earlier versions read field 23. */
static const char *const REAL_LINE =
    "1 (sh) S 0 1 1 0 -1 4194560 1255 0 3 0 1 2 0 0 20 0 1 0 3525328 1691648 184 "
    "18446744073709551615 96610641862656 96610642487267 140723650393056 0 0 0 0 4 "
    "65538 1 0 0 17 0 0 0 0 0 0 96610642630704 96610642645040 96610962841600 "
    "140723650399842 140723650399916 140723650399916 140723650400240 0\n";

static void test_parses_a_real_stat_line(void)
{
    proc_sample s = {0};
    CHECK(proc_parse_stat(REAL_LINE, &s) == 0);
    CHECK(s.pid == 1);
    CHECK(strcmp(s.name, "sh") == 0);
    CHECK(s.state == 'S');
    CHECK(s.cpu_ticks == 3ULL);  /* utime 1 + stime 2 */
    CHECK(s.rss_pages == 184L);  /* not 1691648, the virtual size */
}

static void test_names_with_spaces_and_parentheses(void)
{
    const char *line = "4242 (my (odd) proc) R 1 4242 4242 0 -1 4194304 10 0 0 0 250 50 "
                       "0 0 20 0 3 0 99 123456789 2048 0\n";
    proc_sample s = {0};
    CHECK(proc_parse_stat(line, &s) == 0);
    CHECK(s.pid == 4242);
    CHECK(strcmp(s.name, "my (odd) proc") == 0);
    CHECK(s.state == 'R');
    CHECK(s.cpu_ticks == 300ULL);
    CHECK(s.rss_pages == 2048L);
}

static void test_truncates_overlong_names(void)
{
    char name[300];
    char line[512];
    proc_sample s = {0};

    memset(name, 'x', sizeof name - 1);
    name[sizeof name - 1] = '\0';
    snprintf(line, sizeof line, "7 (%s) S 0 1 1 0 -1 0 0 0 0 0 5 5 0 0 20 0 1 0 1 1 9\n", name);
    CHECK(proc_parse_stat(line, &s) == 0);
    CHECK(strlen(s.name) == PROC_NAME_MAX - 1);
    CHECK(s.rss_pages == 9L);
}

static void test_rejects_malformed_lines(void)
{
    proc_sample s = {0};
    CHECK(proc_parse_stat("", &s) != 0);
    CHECK(proc_parse_stat("not a stat line", &s) != 0);
    CHECK(proc_parse_stat("12 (no closing parenthesis R 1 2 3", &s) != 0);
    CHECK(proc_parse_stat("12 (truncated) S 1 2 3\n", &s) != 0);
    CHECK(proc_parse_stat("abc (sh) S 0 1 1 0 -1 0 0 0 0 0 1 2 0 0 20 0 1 0 1 1 9\n", &s) != 0);
}

static void test_cpu_percent(void)
{
    CHECK(close_to(proc_cpu_percent(100, 150, 100, 1.0), 50.0));  /* half of one core */
    CHECK(close_to(proc_cpu_percent(0, 400, 100, 2.0), 200.0));   /* two busy threads */
    CHECK(close_to(proc_cpu_percent(100, 100, 100, 1.0), 0.0));   /* idle */
    CHECK(close_to(proc_cpu_percent(200, 100, 100, 1.0), 0.0));   /* PID reused */
    CHECK(close_to(proc_cpu_percent(0, 100, 0, 1.0), 0.0));       /* bad tick rate */
    CHECK(close_to(proc_cpu_percent(0, 100, 100, 0.0), 0.0));     /* no time passed */
}

static void test_rss_kib(void)
{
    CHECK(proc_rss_kib(184, 4096) == 736L);
    CHECK(proc_rss_kib(10, 16384) == 160L);
    CHECK(proc_rss_kib(0, 4096) == 0L);
    CHECK(proc_rss_kib(-1, 4096) == 0L);
}

static void test_reads_from_a_proc_tree(void)
{
    proc_sample s = {0};
    CHECK(proc_read_sample("tests/fixtures/proc", 4242, &s) == 0);
    CHECK(strcmp(s.name, "my (odd) proc") == 0);
    CHECK(proc_read_sample("tests/fixtures/proc", 99999, &s) != 0); /* no such process */
}

int main(void)
{
    test_parses_a_real_stat_line();
    test_names_with_spaces_and_parentheses();
    test_truncates_overlong_names();
    test_rejects_malformed_lines();
    test_cpu_percent();
    test_rss_kib();
    test_reads_from_a_proc_tree();

    if (failures > 0) {
        fprintf(stderr, "%d of %d checks failed\n", failures, checks);
        return 1;
    }
    printf("All %d checks passed\n", checks);
    return 0;
}
