#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "hash.h"

#define FLTFMT "%.4g"

#define streql(a,b) (strcmp(a, b) == 0)

#define FATAL_ERROR(arg, rc) do { \
  print_fatal_error((arg), (rc)); \
  status = EXIT_FAILURE; \
  goto cleanup; \
} while (0)

static int istty = 0;

static char *prompt_input (char *buf, size_t size) {
  if (istty) fprintf(stderr, "> ");
  return fgets(buf, size, stdin);
}

static int request_exit (char *s) {
  return istty && (
    streql(s, "q") || streql(s, "x") ||
    streql(s, "quit") || streql(s, "exit"));
}

static void print_fatal_error (char *arg, int rc) {
  fprintf(stderr, "fatal error \"%s\" while processing ",
    hashset_strcode(rc, 0));
  if (arg) fprintf(stderr, "\"%s\"", arg);
  else fprintf(stderr, "command");
  fprintf(stderr, ",  exiting\n");
}

int main (int argc, char **argv) {
  char buf[8192];
  const char *sep = " \t\r\n";
  HashSet *H = hashset_new(NULL, NULL, HASH_INTERN, NULL);
  int cols = argc > 1 ? atoi(argv[1]) / 2 : 40;
  int status = EXIT_SUCCESS;
  istty = isatty(0);
  if (H == NULL) return EXIT_FAILURE;
  while (prompt_input(buf, sizeof buf)) {
    char *cmd = strtok(buf, sep);
    char *arg;
    if (cmd == NULL) continue;
    if (request_exit(cmd)) break;
    arg = strtok(NULL, sep);
    if (arg) {
      int rc;
      switch (cmd[0]) {
        case 'a': rc = hashset_add(H, arg);   break;
        case 't': rc = hashset_test(H, arg);  break;
        case 'd': rc = hashset_del(H, arg);   break;
        default:
          fprintf(stderr, "bad command: %s\n", cmd);
          continue;
      }
      if (rc == H_OK) {
        if (istty) fprintf(stderr, "ok\n");
      }
      else if (rc < 0 || rc >= NRETURNCODES)
        fprintf(stderr, "unknown error code: %d\n", rc);
      else if (rc < H_ERROR)
        fprintf(stderr, "%s: %s\n", hashset_strcode(rc, 0), arg);
      else
        FATAL_ERROR(arg, rc);
    }
    else {
      switch (cmd[0]) {
        case 'z':
          fprintf(stderr, "size: %u\n", hashset_size(H));
          break;
        case 'c':
          fprintf(stderr, "capacity: %u\n", hashset_capacity(H));
          break;
        case 'l':
          fprintf(stderr, "load factor: "FLTFMT"\n",
            hashset_loadfactor(H));
          break;
        case 'p':
          fprintf(stderr, "average probe distance: "FLTFMT"\n",
            hashset_avgprobedist(H));
          break;
        case 'm':
          fprintf(stderr, "maximum probe distance: %d\n",
            hashset_maxprobedist(H));
          break;
        case 'P': {
          int *tbl;
          hash_t i, size = 0;
          int rc = hashset_tblprobedist(H, &size, &tbl);
          if (rc != H_OK) FATAL_ERROR(arg, rc);
          fprintf(stderr, "probe distance table (%u):", hashset_capacity(H));
          for (i = 0; i < size; i++) {
            const char *delim = ((i % cols == 0) ? "\n" : " ");
            fprintf(stderr, "%s%d", delim, tbl[i]);
          }
          fprintf(stderr, "\n");
          free(tbl);
          break;
        }
        default:
          fprintf(stderr, "no arg\n");
      }
    }
  }
cleanup:
  hashset_destroy(H);
  return status;
}
