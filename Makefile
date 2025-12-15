CC = gcc
CFLAGS = -Wall -Wextra -O2
NAME = proc_monitor

all:
	$(CC) $(CFLAGS) main.c -o $(NAME)

clean:
	rm -f $(NAME)
