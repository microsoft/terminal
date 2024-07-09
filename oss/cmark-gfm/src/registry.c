#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "cmark-gfm.h"
#include "syntax_extension.h"
#include "registry.h"
#include "plugin.h"

extern cmark_mem CMARK_DEFAULT_MEM_ALLOCATOR;

static cmark_llist *syntax_extensions = NULL;

void cmark_register_plugin(cmark_plugin_init_func reg_fn) {
  cmark_plugin *plugin = cmark_plugin_new();

  if (!reg_fn(plugin)) {
    cmark_plugin_free(plugin);
    return;
  }

  cmark_llist *syntax_extensions_list = cmark_plugin_steal_syntax_extensions(plugin),
              *it;

  for (it = syntax_extensions_list; it; it = it->next) {
    syntax_extensions = cmark_llist_append(&CMARK_DEFAULT_MEM_ALLOCATOR, syntax_extensions, it->data);
  }

  cmark_llist_free(&CMARK_DEFAULT_MEM_ALLOCATOR, syntax_extensions_list);
  cmark_plugin_free(plugin);
}

void cmark_release_plugins(void) {
  if (syntax_extensions) {
    cmark_llist_free_full(
        &CMARK_DEFAULT_MEM_ALLOCATOR,
        syntax_extensions,
        (cmark_free_func) cmark_syntax_extension_free);
    syntax_extensions = NULL;
  }
}

cmark_llist *cmark_list_syntax_extensions(cmark_mem *mem) {
  cmark_llist *it;
  cmark_llist *res = NULL;

  for (it = syntax_extensions; it; it = it->next) {
    res = cmark_llist_append(mem, res, it->data);
  }
  return res;
}

cmark_syntax_extension *cmark_find_syntax_extension(const char *name) {
  cmark_llist *tmp;

  for (tmp = syntax_extensions; tmp; tmp = tmp->next) {
    cmark_syntax_extension *ext = (cmark_syntax_extension *) tmp->data;
    if (!strcmp(ext->name, name))
      return ext;
  }
  return NULL;
}
