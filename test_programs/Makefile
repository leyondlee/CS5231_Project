CC = gcc
CFLAGS = -Wall -fno-stack-protector

PROGRAMS = function_ptr heap jit_test longjmp strcpy_overflow

all: $(PROGRAMS)

%: %.c
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f $(PROGRAMS)
