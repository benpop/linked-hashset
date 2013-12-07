#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "hash.h"

static int results (int *pxrc, int rc, const char *key, const char *cmd) {
  if (rc != H_OK) {
    *pxrc = rc;
    fprintf(stderr, "%s: %s %s\n", hashset_strcode(rc), cmd, key);
  }
  return rc;
}

/* int main (int argc, char *argv[]) { */
int main (void) {
  char buf[8192];
  HashSet *H = hashset_new(NULL, NULL, HASH_INTERN, NULL);
  if (H == NULL) return 1;
  while (fgets(buf, sizeof buf, stdin)) {
    char *key = strtok(buf, " ");
    int cmd = key[0];
    int xrc;
    if (cmd == 'q') break;  /* quit */
    while ((key = strtok(NULL, " ")) != NULL) {
      int rc;
      switch (tolower(cmd)) {
        case 'a': {
          rc = hashset_add(H, key);
          break;
        }
        case 't': case 'g': {
          rc = hashset_test(H, key);
          break;
        }
        case 'd': {
          rc = hashset_del(H, key);
          break;
        }
        default: {
          fprintf(stderr, "Invalid command: %s\n", buf);
          rc = H_ERROR;
        }
      }
      results(&xrc, rc, key, buf);
    }
    if (xrc == H_OK) fprintf(stderr, "ok.\n");
  }
cleanup:
  hashset_destroy(H);
  return 0;
}
