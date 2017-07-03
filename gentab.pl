#!/usr/bin/env perl

# usage: ./gentab.pl < unistd.h > output.h

print "typedef struct syscall {
  char *name;
  short code;
} Syscall;
Syscall systab[] = {\n";

print
  join ",\n",
  map { /__NR_(\w+)\s+(\d+)/ ? qq(  { "$1", $2 }) : () } <>;

print "\n};\n";
