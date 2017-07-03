#!/bin/sh -eu

# syscall(1) test suite

S=./syscall
T() { echo " $*"; }

###

cat /etc/issue.net || true
$S -v
echo

T hello world
test $($S write 1 "hello_world\n" \#hello_world_) = hello_world

T builtin echo
test $($S open /dev/random 0 , echo \$0 , close \$0) -gt 2

T repeating 100 times
$S -100 write 1 x 1 >/dev/null

echo
echo ALL OK
echo
