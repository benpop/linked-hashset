CC = clang
AR = ar rcu
RANLIB = ranlib
RM = rm -f

CFLAGS = -Wall -Wextra -ansi -pedantic -DHAVE_STRDUP

N = hash
C = $N.c
H = $N.h
O = $N.o
A = lib$N.a

all: $A

test: test.c $A
	$(CC) $(CFLAGS) -o $@ $< -I. -L. -l$N

clean:
	$(RM) $O $A

lib%.a: %.o
	$(AR) $@ $<
	$(RANLIB) $@

%.o: %.c %.h
	$(CC) $(CFLAGS) -c -o $@ $<
