CC = clang
AR = ar rcu
RANLIB = ranlib
RM = rm -f

CFLAGS = -Wall -Wextra -ansi -pedantic -DHAVE_STRDUP
LFLAGS = -Wall -Wextra

N = hash
C = $N.c
H = $N.h
O = $N.o
A = lib$N.a

all: $A

test: test.o $A
	$(CC) $(LFLAGS) -o $@ $< -L. -l$N

clean:
	$(RM) *.o $A test test.out

lib%.a: %.o
	$(AR) $@ $<
	$(RANLIB) $@

%.o: %.c $H
	$(CC) $(CFLAGS) -c $<
