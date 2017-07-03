/*
  syscall - Launch system calls from your shell
  Copyright (c) 2017, Olivier Duclos

  Permission to use, copy, modify, and/or distribute this software for any
  purpose with or without fee is hereby granted, provided that the above
  copyright notice and this permission notice appear in all copies.

  THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
  REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
  AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
  INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
  LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
  OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
  PERFORMANCE OF THIS SOFTWARE.
*/

#include <err.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "systab.h"

#define LEN(x)      (sizeof(x) / sizeof(*x))
#define streq(a,b)  (strcmp(a,b) == 0)

#define CMD_MAX   20  /* maximum number of commands per invocation */

static int ret_values[CMD_MAX];

void usage()
{
  puts("usage: syscall [-<n>] name [args...] [, name [args...]]...");
}

int scomp(const void *m1, const void *m2)
{
  Syscall *sys1 = (Syscall *)m1;
  Syscall *sys2 = (Syscall *)m2;
  return strcmp(sys1->name, sys2->name);
}

long str2syscall(const char *name)
{
  Syscall key, *res;

  key.name = (char *)name;

  res = bsearch(&key, systab, LEN(systab), sizeof key, scomp);
  if (res == NULL) {
    errx(1, "unknown system call: %s", name);
  }

  return (long)res->code;
}

/* Quick and dirty way to unescape \n at the end of strings */
void unescape_nl(char *str) {
  size_t end = strlen(str) - 1;

  while (str[end] == 'n' && str[end-1] == '\\') {
    str[end-1] = '\n';
    str[end] = '\0';
    end -= 2;
  }
}

unsigned long parse_arg(const char *syscall_name, char *arg)
{
  unsigned long num;
  char *endp = NULL;

  /* #hello - length of a string */
  if (arg[0] == '#') {
    return (unsigned long)strlen(arg + 1);
  }

  /* $0 - return value of a previous syscall */
  if (arg[0] == '$') {
    num = strtoul(arg + 1, &endp, 10);
    if (num < CMD_MAX - 1) {
      return (unsigned long)ret_values[num];
    }
  }

  /* Try to parse it as a number */
  endp = NULL;
  num = strtoul(arg, &endp, 0);
  if (errno == ERANGE) {
    errx(1, "%s: argument '%s' is out of range",
      syscall_name, arg);
  }
  if (endp && *endp == '\0') {
    /* strtoul succeeded */
    return num;
  }

  /* assume it is a string */
  /* unescape any \n at the end of the string */
  unescape_nl(arg);
  return (unsigned long)arg;
}

void echo(int argc, char **argv) {
  int i;

  for (i = 1; i < argc; i++) {
    if (argv[i][0] == '$' || argv[i][0] == '#') {
      printf("%ld\n", parse_arg(__func__, argv[i]));
    } else {
      printf("%s\n", argv[i]);
    }
  }
}

#define ARG(n)    parse_arg(syscall_name, cmd[n])

void parse_syscall(int cmd_no, char **cmd, int cmd_len)
{
  long syscall_num, ret = -1;
  char *syscall_name = cmd[0];

  /* Special case */
  if (streq(syscall_name, "echo")) {
    echo(cmd_len, cmd);
    ret_values[cmd_no] = 0; /* echo never fails */
    return;
  }

  syscall_num = str2syscall(syscall_name);
  if (syscall_num == -1) {
    errx(1, "unknown system call '%s'", syscall_name);
  }

  switch (cmd_len) {
  case 0:
    ret = syscall(syscall_num);
    break;
  case 1:
    ret = syscall(syscall_num, ARG(1));
    break;
  case 2:
    ret = syscall(syscall_num, ARG(1), ARG(2));
    break;
  case 3:
    ret = syscall(syscall_num, ARG(1), ARG(2), ARG(3));
    break;
  case 4:
    ret = syscall(syscall_num, ARG(1), ARG(2), ARG(3), ARG(4));
    break;
  case 5:
    ret = syscall(syscall_num, ARG(1), ARG(2), ARG(3), ARG(4), ARG(5));
    break;
  default:
    errx(1, "too many arguments for %s", syscall_name);
  }

  if (ret == -1) {
    errx(1, "%s failed: %s", syscall_name, strerror(errno));
  }

  ret_values[cmd_no] = (int)ret;
}

void split_cmdline(int argc, char **argv)
{
  int cmd_no = 0, cmd_len = 0, i;

  for (i = 0; i < argc; i++) {
    //printf("dbg: %s\n", argv[i]);
    if (cmd_no == CMD_MAX) {
      errx(1, "too many command (%d > %d)", cmd_no, CMD_MAX);
    }
    if (streq(argv[i], ",") || i == argc-1) {
      //printf("parse: len=%d  no=%d  start=%s\n", cmd_len, cmd_no, (argv + (i - cmd_len))[0]);
      parse_syscall(cmd_no, argv + (i - cmd_len), cmd_len);
      cmd_no++;
      cmd_len = 0;
    } else {
      cmd_len++;
    }
  }
}

/* for debugging */
void dump_ret_values(void) {
  int i;

  for (i = 0; i < CMD_MAX; i++) {
    printf("%0d  %d\n", i, ret_values[i]);
  }
}

int main(int argc, char **argv)
{
  int repeat = 1, skip = 1;

  if (argc <= 1) {
    usage();
    return 0;
  }

  /* arguments */
  if (argv[1][0] == '-') {
    /* help */
    if (streq(argv[1], "-h") || streq(argv[1], "--help")) {
      usage();
      return 0;
    }

    /* version */
    if (streq(argv[1], "-v") || streq(argv[1], "--version")) {
      puts("syscall version "VERSION);
      return 0;
    }

    /* Handle -n option */
    repeat = atoi(argv[1] + 1);
    if (repeat < 1) {
      errx(1, "option -<n> must be between 1 and %d", INT_MAX);
    }
    if (argc <= 3) {
      usage();
      return 1;
    }
    skip++;
  }

  memset(ret_values, -1, sizeof ret_values);

  /* Prepare the syscalls table for searching */
  qsort(systab, LEN(systab), sizeof(Syscall), scomp);

  while (repeat--) {
    split_cmdline(argc - skip, argv + skip);
  }

  return 0;
}
