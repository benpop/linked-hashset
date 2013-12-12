#!/bin/sh

TMP=`mktemp /tmp/hashsettest.XXXXXX` || exit 1

sed -e 's/^/a /' /usr/share/dict/words > "$TMP"

make test

cat "$TMP" test.txt | ./test 2>&1 | sed -e '/^ok$/d' > test.out
