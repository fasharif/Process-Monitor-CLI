CC       ?= cc
CFLAGS   ?= -O2
WARNINGS := -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Werror
CPPFLAGS += -Iinclude
ALL_CFLAGS = -std=c11 $(WARNINGS) $(CFLAGS)

NAME     := proc_monitor
SRC      := src/main.c src/proc.c
OBJ      := $(SRC:.c=.o)
TEST_BIN := tests/test_proc
SANITIZE := -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer

.PHONY: all test integration sanitize clean fclean re

all: $(NAME)

$(NAME): $(OBJ)
	$(CC) $(ALL_CFLAGS) $(LDFLAGS) -o $@ $(OBJ)

src/%.o: src/%.c include/proc.h
	$(CC) $(CPPFLAGS) $(ALL_CFLAGS) -c -o $@ $<

$(TEST_BIN): tests/test_proc.c src/proc.c include/proc.h
	$(CC) $(CPPFLAGS) $(ALL_CFLAGS) $(LDFLAGS) -o $@ tests/test_proc.c src/proc.c

test: $(TEST_BIN)
	./$(TEST_BIN)

integration: $(NAME)
	sh tests/run_integration.sh

sanitize: fclean
	$(MAKE) test integration CFLAGS="$(SANITIZE)" LDFLAGS="$(SANITIZE)"

clean:
	rm -f $(OBJ) $(TEST_BIN)

fclean: clean
	rm -f $(NAME)

re: fclean all
