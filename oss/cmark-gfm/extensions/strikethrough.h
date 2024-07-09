#ifndef CMARK_GFM_STRIKETHROUGH_H
#define CMARK_GFM_STRIKETHROUGH_H

#include "cmark-gfm-core-extensions.h"

extern cmark_node_type CMARK_NODE_STRIKETHROUGH;
cmark_syntax_extension *create_strikethrough_extension(void);

#endif
