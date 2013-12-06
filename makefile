CC = clang
AR = ar rcu
RANLIB = ranlib
RM = rm -f

CFLAGS = -Wall -Wextra

N = hash
C = $N.c
H = $N.h
O = $N.o
A = lib$N.a

all: $A

clean:
	$(RM) $O $A

lib%.a: %.o
	$(AR) $@ $<
	$(RANLIB) $@

%.o: %.c %.h
	$(CC) $(CFLAGS) -c -o $@ $<
