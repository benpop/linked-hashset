#include <stdlib.h>
#include <string.h>

#include "hash.h"

#define INIT_CAP 8

#define MAX_LOAD_PERCENT 0.75

#define FNV_OFFSET_BASIS  2166136261UL
#define FNV_PRIME         16777619UL

#define HASH_MASK(tab) ((tab)->cap-1)
#define HASH_MOD(tab,hash) (HASH_MASK(tab) & (hash))
#define HASH_GET(tab,idx) ((tab)->buckets[(idx)])
#define HASH_LOOKUP(tab,hash) HASH_GET((tab),HASH_MOD((tab),(hash)))

#define HASH_LOADFACTOR(tab) ((double)(tab)->numElts / (tab)->cap)
#define HASH_AVGPROBEDIST(tab) ((double)(tab)->numElts / (tab)->numBuckets)

#define HASH_INTERN_FREE(ptr) free((void *)(ptr))

typedef struct Elt {
  hash_t hash;
  const char *key;
  struct Elt *next;
} Elt;

struct HashSet {
  hash_t cap;  /* number of buckets in main table */
  hash_t numElts;  /* number of elements in use */
  hash_t numBuckets;  /* number of buckets used */
  HashFunc hashFunc;
  EqualFunc equalFunc;
  DestroyFunc destroyFunc;
  void *ud;  /* user data */
  Elt **buckets;
};

static const char * const HCodeShortMsgs[] = {
  "ok", "not found", "exists", "error", "abort",
  "invalid", "out of memory",
  NULL
};

static const char * const HCodeLongMsgs[] = {
  "ok", "item not found", "item exists", "error", "aborted by caller",
  "invalid argument(s)", "out of memory",
  NULL
};

const char *hashset_strcode (int rc, int longMsg) {
  if (rc >= 0 && rc < NRETURNCODES)
    return (longMsg ? HCodeLongMsgs : HCodeShortMsgs)[rc];
  else return NULL;
}

/*
http://en.wikipedia.org/wiki/Jenkins_hash_function
*/
hash_t hash_jenkins (const char *key, void *ud) {
  size_t i;
  hash_t hash = 0;
  (void)ud;
  for (i = 0; key[i] != '\0'; i++) {
    hash += (unsigned char)key[i];
    hash += (hash << 10);
    hash ^= (hash >> 6);
  }
  hash += (hash << 3);
  hash ^= (hash >> 11);
  hash += (hash << 15);
  return hash;
}

/*
http://www.isthe.com/chongo/tech/comp/fnv/
*/
hash_t hash_fnv1 (const char *key, void *ud) {
  size_t i;
  hash_t hash = FNV_OFFSET_BASIS;
  (void)ud;
  for (i = 0; key[i] != '\0' && hash != 0; i++) {
    hash *= FNV_PRIME;
    hash ^= (unsigned char)key[i];
  }
  return hash;
}

/*
http://www.isthe.com/chongo/tech/comp/fnv/
*/
hash_t hash_fnv1a (const char *key, void *ud) {
  size_t i;
  hash_t hash = FNV_OFFSET_BASIS;
  (void)ud;
  for (i = 0; key[i] != '\0' && hash != 0; i++) {
    hash ^= (unsigned char)key[i];
    hash *= FNV_PRIME;
  }
  return hash;
}

HashSet *hashset_new (
    HashFunc hashFunc, EqualFunc equalFunc,
    DestroyFunc destroyFunc, void *ud) {
  HashSet *H = malloc(sizeof *H);
  if (H == NULL) return NULL;
  H->buckets = calloc(INIT_CAP, sizeof *H->buckets);
  if (H->buckets == NULL) { free(H); return NULL; }
  H->cap = INIT_CAP;
  H->numElts = 0;
  H->numBuckets = 0;
  H->hashFunc = hashFunc ? hashFunc : hash_jenkins;
  H->equalFunc = equalFunc;
  H->destroyFunc = destroyFunc;
  H->ud = ud;
  return H;
}

void hashset_destroy (HashSet *H) {
  if (H) {
    hash_t i;
    for (i = 0; i < H->cap; i++) {
      Elt *p = NULL;  /* parent */
      Elt *e = HASH_GET(H, i);
      for ( ; e; p = e, e = e->next) {
        if (H->destroyFunc) {
          if (H->destroyFunc == HASH_INTERN)
            HASH_INTERN_FREE(e->key);
          else
            H->destroyFunc(e->key, H->ud);
        }
        free(p);
      }
    }
    free(H->buckets);
    free(H);
  }
}

void hashset_values (HashSet *H,
    HashFunc *pHashFunc, EqualFunc *pEqualFunc,
    DestroyFunc *pDestroyFunc, void **pUd) {
  if (H) {
    if (pHashFunc) *pHashFunc = H->hashFunc;
    if (pEqualFunc) *pEqualFunc = H->equalFunc;
    if (pDestroyFunc) *pDestroyFunc = H->destroyFunc;
    if (pUd) *pUd = H->ud;
  }
}

#ifndef HAVE_STRDUP
static char *strdup (const char *src) {
  size_t n = strlen(src);
  char *copy = malloc(n + 1);
  if (copy == NULL) return NULL;
  return strncpy(copy, src, n);
}
#endif

static int items_equal (HashSet *H, const char *a, const char *b) {
  if (a == b) return 1;  /* pointer equality */
  else if (H->equalFunc) return H->equalFunc(a, b, H->ud);
  else return strcmp(a, b) == 0;
}

static Elt *elt_new (HashSet *H, const char *item, hash_t hash, Elt *next) {
  Elt *e;
  int intern = (H->destroyFunc == HASH_INTERN);
  if (intern) {
    const char *copy = strdup(item);
    if (copy == NULL) return NULL;
    else item = copy;
  }
  e = malloc(sizeof *e);
  if (e == NULL) {
    if (intern) HASH_INTERN_FREE(item);
    return NULL;
  }
  e->key = item;
  e->hash = hash;
  e->next = next;
  return e;
}

static int rehash (HashSet *H) {
  Elt **oldBuckets = H->buckets;
  hash_t newNumBuckets = 0;
  hash_t oldCap = H->cap;
  hash_t i;
  int rc;
  H->cap <<= 1;  /* next power of 2 */
  if (H->cap <= oldCap) {  /* size overflow */
    rc = H_TOOBIG;
    goto cleanup;
  }
  H->buckets = calloc(H->cap, sizeof *H->buckets);
  if (H->buckets == NULL) {  /* out of memory */
    rc = H_NOMEM;
    goto cleanup;
  }
  for (i = 0; i < oldCap; i++) {  /* rehash elements */
    Elt *e = oldBuckets[i];
    while (e) {
      /* XXX: current implementation places elements in reverse order */
      Elt **bucket = &HASH_LOOKUP(H, e->hash);
      Elt *next = e->next;
      if (*bucket == NULL) newNumBuckets++;
      e->next = *bucket;
      *bucket = e;
      e = next;
    }
  }
  H->numBuckets = newNumBuckets;
  free(oldBuckets);
  return H_OK;
cleanup:
  H->cap = oldCap;
  H->buckets = oldBuckets;
  return rc;
}

int hashset_add (HashSet *H, const char *item) {
  Elt **bucket, *e;
  hash_t itemHash;
  if (H == NULL || item == NULL) return H_INVALID;
  if (HASH_LOADFACTOR(H) >= MAX_LOAD_PERCENT) {
    int rc = rehash(H);
    if (rc != H_OK) return rc;
  }
  itemHash = H->hashFunc(item, H->ud);
  bucket = &HASH_LOOKUP(H, itemHash);
  for (e = *bucket; e; e = e->next) {
    if (itemHash == e->hash && items_equal(H, item, e->key))
      return H_EXISTS;
  }
  /* item not found, insert at head of bucket list */
  e = elt_new(H, item, itemHash, *bucket);
  if (e) {
    if (*bucket == NULL) H->numBuckets++;
    H->numElts++;
    *bucket = e;
    return H_OK;
  }
  else return H_NOMEM;
}

int hashset_test (HashSet *H, const char *item) {
  Elt *e;
  hash_t itemHash;
  if (H == NULL || item == NULL) return H_INVALID;
  itemHash = H->hashFunc(item, H->ud);
  e = HASH_LOOKUP(H, itemHash);
  for ( ; e; e = e->next) {
    if (itemHash == e->hash && items_equal(H, item, e->key))
      return H_OK;
  }
  return H_NOTFOUND;
}

int hashset_del (HashSet *H, const char *item) {
  Elt **bucket, *e, *p = NULL;
  hash_t itemHash;
  if (H == NULL || item == NULL) return H_INVALID;
  itemHash = H->hashFunc(item, H->ud);
  bucket = &HASH_LOOKUP(H, itemHash);
  for (e = *bucket; e; p = e, e = e->next) {
    if (itemHash == e->hash && items_equal(H, item, e->key))
      goto delete;
  }
  return H_NOTFOUND;
delete:
  if (H->destroyFunc == HASH_INTERN)
    HASH_INTERN_FREE(e->key);
  else if (H->destroyFunc)
    H->destroyFunc(e->key, H->ud);
  /* unlink element */
  if (p) p->next = e->next;
  else *bucket = e->next;
  free(e);
  if (*bucket == NULL) H->numBuckets--;
  H->numElts--;
  return H_OK;
}

double hashset_loadfactor (HashSet *H) {
  return H ? HASH_LOADFACTOR(H) : -1.0;
}

double hashset_avgprobedist (HashSet *H) {
  return H ? HASH_AVGPROBEDIST(H) : -1.0;
}

int hashset_tblprobedist (HashSet *H, hash_t *pSize, int **pTbl) {
  int *tbl;
  hash_t i;
  if (H == NULL || pSize == NULL || pTbl == NULL)
    return H_INVALID;
  if (*pSize < H->cap) {
    tbl = malloc(H->cap * sizeof *tbl);
    if (tbl == NULL) return H_NOMEM;
  }
  for (i = 0; i < H->cap; i++) {
    Elt *e = HASH_GET(H, i);
    int n = 0;
    for ( ; e; e = e->next) n++;
    tbl[i] = n;
  }
  *pSize = H->cap;
  *pTbl = tbl;
  return H_OK;
}

