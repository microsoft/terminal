#ifndef CMARK_FOOTNOTES_H
#define CMARK_FOOTNOTES_H

#include "map.h"

#ifdef __cplusplus
extern "C" {
#endif

struct cmark_footnote {
  cmark_map_entry entry;
  cmark_node *node;
  unsigned int ix;
};

typedef struct cmark_footnote cmark_footnote;

void cmark_footnote_create(cmark_map *map, cmark_node *node);
cmark_map *cmark_footnote_map_new(cmark_mem *mem);

void cmark_unlink_footnotes_map(cmark_map *map);

#ifdef __cplusplus
}
#endif

#endif
