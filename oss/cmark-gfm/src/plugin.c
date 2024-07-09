#include <stdlib.h>

#include "plugin.h"

extern cmark_mem CMARK_DEFAULT_MEM_ALLOCATOR;

int cmark_plugin_register_syntax_extension(cmark_plugin    * plugin,
                                        cmark_syntax_extension * extension) {
  plugin->syntax_extensions = cmark_llist_append(&CMARK_DEFAULT_MEM_ALLOCATOR, plugin->syntax_extensions, extension);
  return 1;
}

cmark_plugin *
cmark_plugin_new(void) {
  cmark_plugin *res = (cmark_plugin *) CMARK_DEFAULT_MEM_ALLOCATOR.calloc(1, sizeof(cmark_plugin));

  res->syntax_extensions = NULL;

  return res;
}

void
cmark_plugin_free(cmark_plugin *plugin) {
  cmark_llist_free_full(&CMARK_DEFAULT_MEM_ALLOCATOR,
                        plugin->syntax_extensions,
                        (cmark_free_func) cmark_syntax_extension_free);
  CMARK_DEFAULT_MEM_ALLOCATOR.free(plugin);
}

cmark_llist *
cmark_plugin_steal_syntax_extensions(cmark_plugin *plugin) {
  cmark_llist *res = plugin->syntax_extensions;

  plugin->syntax_extensions = NULL;
  return res;
}
