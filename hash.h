#ifndef blhash_h
#define blhash_h

#include <stddef.h>

#ifndef HASH_T
#if defined(HAVE_STDINT_H) || \
  (defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L)
#include <stdint.h>
typedef uint32_t hash_t;
#else
typedef unsigned int hash_t;
#endif
#define HASH_T hash_t
#endif

typedef hash_t (*HashFunc) (const char *key, void *ud);
typedef int (*EqualFunc) (const char *a, const char *b, void *ud);
typedef void (*DestroyFunc) (const char *item, void *ud);

typedef struct HashSet HashSet;

/*
Duplicate strings added and hold them internally.
*/
#define HASH_INTERN ((DestroyFunc)-1)
/*#define HASH_NOCASE ((EqualFunc)-1)*/

enum HReturnCodes {
  H_OK = 0,
  H_NOTFOUND,
  H_EXISTS,
  H_ERROR,
  H_ABORT,
  H_INVALID,
  H_NOMEM,
  NRETURNCODES
};

hash_t hash_jenkins (const char *key, void *ud);
hash_t hash_fnv1    (const char *key, void *ud);
hash_t hash_fnv1a   (const char *key, void *ud);

HashSet *hashset_new (
    HashFunc hashFunc, EqualFunc equalFunc,
    DestroyFunc destroyFunc, void *ud);

void hashset_destroy (HashSet*);

void hashset_values (HashSet*,
    HashFunc *pHashFunc, EqualFunc *pEqualFunc,
    DestroyFunc *pDestroyFunc, void **pUd);

int hashset_add (HashSet*, const char *item);

int hashset_test (HashSet*, const char *item);

int hashset_del (HashSet*, const char *item);

#endif
