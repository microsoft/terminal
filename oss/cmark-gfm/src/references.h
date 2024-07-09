#ifndef CMARK_REFERENCES_H
#define CMARK_REFERENCES_H

#include "map.h"

#ifdef __cplusplus
extern "C" {
#endif

struct cmark_reference {
  cmark_map_entry entry;
  cmark_chunk url;
  cmark_chunk title;
};

typedef struct cmark_reference cmark_reference;

void cmark_reference_create(cmark_map *map, cmark_chunk *label,
                            cmark_chunk *url, cmark_chunk *title);
cmark_map *cmark_reference_map_new(cmark_mem *mem);

#ifdef __cplusplus
}
#endif

#endif
