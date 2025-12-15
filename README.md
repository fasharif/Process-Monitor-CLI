# Process-Monitor-CLI
A lightweight Linux command-line program that reads /proc and shows CPU & memory usage for each running process — similar to a tiny top.
This highlights your systems and green awareness (seeing what uses energy) 🖥️.

✅ Run it:

make

./proc_monitor | head -20

You’ll see:

PID    NAME                      CPU      MEMORY
--------------------------------------------------------------
1      systemd                   0.00%    3456 KB
42     bash                      0.10%    2048 KB

This reads /proc, calculates basic CPU usage, and prints memory (RSS) — a minimalist “green” system monitor.
