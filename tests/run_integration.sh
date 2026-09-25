#!/bin/sh
# End-to-end checks for proc_monitor. Run from the repository root: make integration
set -eu

bin=./proc_monitor
fixtures=tests/fixtures/proc
page_kib=$(( $(getconf PAGESIZE) / 1024 ))

fail() {
    echo "FAIL: $*" >&2
    exit 1
}

echo "1. Memory is rss (field 24) x page size, not the virtual size"
out=$("$bin" -p "$fixtures" -i 0.1)
echo "$out"
echo "$out" | awk -v want=$((184 * page_kib)) '
    $1 == 1 { found = 1; if ($3 != want) { printf "PID 1: expected %s KiB, got %s\n", want, $3; exit 1 } }
    END { if (!found) { print "PID 1 missing"; exit 1 } }' || fail "fixture memory reading"
echo "$out" | grep -q ' my (odd) proc$' || fail "name containing parentheses"

echo "2. Live system: the RSS of PID 1 matches ps(1) within 10%"
ours=$("$bin" -i 0.2 | awk '$1 == 1 { print $3 }')
theirs=$(ps -o rss= -p 1 | tr -d ' ')
echo "proc_monitor: ${ours:-?} KiB, ps: ${theirs:-?} KiB"
[ -n "$ours" ] && [ -n "$theirs" ] || fail "missing reading for PID 1"
awk -v a="$ours" -v b="$theirs" 'BEGIN { d = a - b; if (d < 0) d = -d; exit !(b > 0 && d <= b / 10) }' \
    || fail "RSS differs from ps by more than 10%"

echo "3. Invalid options exit with status 2"
for args in "-s bogus" "-i 0" "-i 99" "-n 0" "-n abc" "extra"; do
    status=0
    # shellcheck disable=SC2086 # splitting $args into separate words is intended
    "$bin" $args >/dev/null 2>&1 || status=$?
    [ "$status" -eq 2 ] || fail "'$args' exited with $status, expected 2"
done

echo "4. -n limits the number of rows"
rows=$("$bin" -i 0.1 -n 3 | tail -n +2 | wc -l)
[ "$rows" -eq 3 ] || fail "-n 3 printed $rows rows"

echo "All integration checks passed"
