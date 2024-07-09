#ifndef CMARK_MAP_H
#define CMARK_MAP_H

#include "chunk.h"

#ifdef __cplusplus
extern "C" {
#endif

struct cmark_map_entry {
  struct cmark_map_entry *next;
  unsigned char *label;
  size_t age;
  size_t size;
};

typedef struct cmark_map_entry cmark_map_entry;

struct cmark_map;

typedef void (*cmark_map_free_f)(struct cmark_map *, cmark_map_entry *);

struct cmark_map {
  cmark_mem *mem;
  cmark_map_entry *refs;
  cmark_map_entry **sorted;
  size_t size;
  size_t ref_size;
  size_t max_ref_size;
  cmark_map_free_f free;
};

typedef struct cmark_map cmark_map;

unsigned char *normalize_map_label(cmark_mem *mem, cmark_chunk *ref);
cmark_map *cmark_map_new(cmark_mem *mem, cmark_map_free_f free);
void cmark_map_free(cmark_map *map);
cmark_map_entry *cmark_map_lookup(cmark_map *map, cmark_chunk *label);

#ifdef __cplusplus
}
#endif

#endif
