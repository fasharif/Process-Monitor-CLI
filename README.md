# Process-Monitor-CLI

[![CI](https://github.com/fasharif/Process-Monitor-CLI/actions/workflows/ci.yml/badge.svg)](https://github.com/fasharif/Process-Monitor-CLI/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)

A small `top`-style process monitor for Linux, written in C. It reads `/proc` directly and
lists each process's CPU and memory use, sorted by whichever you care about.

```
$ ./proc_monitor -n 6
    PID   CPU%   RSS(KiB) S NAME
   1903   28.9     127136 S Runner.Worker
    775    1.0      51196 S php-fpm8.3
    865    1.0      45944 S containerd
   1839    0.0     156364 S provjobd1861240
   1882    0.0      97844 S Runner.Listener
   1017    0.0      72660 S dockerd
```

*Output captured on a GitHub Actions runner.*

## Usage

```bash
make
./proc_monitor               # every process, busiest first, measured over 1 second
./proc_monitor -n 10 -s mem  # the 10 processes using the most memory
./proc_monitor -h            # all options
```

| Option | Meaning | Default |
| --- | --- | --- |
| `-i SECONDS` | How long to measure CPU use for (0.1 to 60) | 1 |
| `-n COUNT` | Show only the first COUNT processes | all |
| `-s cpu` or `-s mem` | Sort by CPU or by memory | `cpu` |

Columns: `PID`, `CPU%` (100% is one full core, as in `top`), `RSS(KiB)` (memory actually held
in RAM), `S` (process state) and `NAME`.

Requires Linux, a C11 compiler and `make`.

## How it works

- **Memory** is the resident set size: field 24 of `/proc/[pid]/stat`, a number of pages,
  multiplied by the page size from `sysconf(_SC_PAGESIZE)`.
- **CPU%** comes from two readings of each process's CPU time (fields 14 and 15, `utime` and
  `stime`) taken `-i` seconds apart: the CPU time used in between, divided by the time that
  passed. This is how `top` measures current use.
- **Names** can contain spaces and parentheses (`my (odd) proc`), so the parser takes the name
  from the first `(` to the last `)` instead of splitting the line on spaces.
- A process that exits between readings is skipped.

## Testing

```bash
make test         # unit tests: /proc parsing, CPU and memory maths
make integration  # runs the program on fixture data and checks its memory figure against ps
make sanitize     # both suites under AddressSanitizer and UndefinedBehaviorSanitizer
```

GitHub Actions runs all three on every push and pull request, builds with both GCC and Clang,
and runs cppcheck.

## Project structure

```
include/proc.h            parser and calculation interface
src/proc.c                /proc/[pid]/stat parser, CPU% and memory calculations
src/main.c                options, sampling, sorting and output
tests/test_proc.c         unit tests
tests/run_integration.sh  end-to-end checks
tests/fixtures/proc/      a small fake /proc tree used by the tests
```

## Limitations

- Linux only, because it reads `/proc`.
- Prints one snapshot and exits; it does not refresh like `top`.
- A process that starts during the measuring interval is shown with 0% CPU.

## License

[MIT](LICENSE)
