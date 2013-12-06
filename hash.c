#include <stdlib.h>
#include <string.h>

#include "hash.h"

#define INIT_CAP 8

#define MAX_LOAD_PERCENT 75

#define FNV_OFFSET_BASIS  2166136261UL
#define FNV_PRIME         16777619UL

#define HASH_MASK(tab) ((tab)->cap-1)
#define HASH_MOD(tab,hash) (HASH_MASK(tab) & (hash))
#define HASH_BUCKET(tab,pos) (&(tab)->buckets[(pos)])

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
  H->hashFunc = hashFunc != NULL ? hashFunc : hash_jenkins;
  H->equalFunc = equalFunc;
  H->destroyFunc = destroyFunc;
  H->ud = ud;
  return H;
}

void hashset_destroy (HashSet *H) {
  if (H != NULL) {
    hash_t i;
    for (i = 0; i < H->cap; i++) {
      Elt *p = NULL;  /* parent */
      Elt *e = H->buckets[i];
      for ( ; e != NULL; p = e, e = e->next) {
        if (H->destroyFunc != NULL) {
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
  if (H != NULL) {
    if (pHashFunc != NULL) *pHashFunc = H->hashFunc;
    if (pEqualFunc != NULL) *pEqualFunc = H->equalFunc;
    if (pDestroyFunc != NULL) *pDestroyFunc = H->destroyFunc;
    if (pUd != NULL) *pUd = H->ud;
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
  else return strcmp(a, b);
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

int hashset_add (HashSet *H, const char *item) {
  Elt **bucket, *e;
  hash_t itemHash, pos;
  if (H == NULL || item == NULL) return H_INVALID;
  itemHash = H->hashFunc(item, H->ud);
  pos = HASH_MOD(H, itemHash);
  bucket = HASH_BUCKET(H, pos);
  for (e = *bucket; e != NULL; e = e->next) {
    if (itemHash == e->hash && items_equal(H, item, e->key))
      return H_EXISTS;
  }
  /* item not found, insert at head of bucket list */
  e = elt_new(H, item, itemHash, *bucket);
  if (e != NULL) *bucket = e;
  else return H_NOMEM;
  return H_OK;
}

int hashset_test (HashSet *H, const char *item) {
  Elt *e;
  hash_t itemHash, pos;
  if (H == NULL || item == NULL) return H_INVALID;
  itemHash = H->hashFunc(item, H->ud);
  pos = HASH_MOD(H, itemHash);
  e = *HASH_BUCKET(H, pos);
  for ( ; e != NULL; e = e->next) {
    if (itemHash == e->hash && items_equal(H, item, e->key))
      return H_OK;
  }
  return H_NOTFOUND;
}

int hashset_del (HashSet *H, const char *item) {
  Elt **bucket, *e, *p;
  hash_t itemHash, pos;
  if (H == NULL || item == NULL) return H_INVALID;
  itemHash = H->hashFunc(item, H->ud);
  pos = HASH_MOD(H, itemHash);
  bucket = HASH_BUCKET(H, pos);
  for (e = *bucket; e != NULL; p = e, e = e->next) {
    if (itemHash == e->hash && items_equal(H, item, e->key))
      goto _del_;
  }
  return H_NOTFOUND;
_del_:
  if (H->destroyFunc == HASH_INTERN)
    HASH_INTERN_FREE(e->key);
  else if (H->destroyFunc)
    H->destroyFunc(e->key, H->ud);
  /* unlink element */
  if (p != NULL) p->next = e->next;
  else *bucket = e->next;
  free(e);
  return H_OK;
}
