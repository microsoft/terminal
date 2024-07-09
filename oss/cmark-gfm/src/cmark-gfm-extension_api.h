#ifndef CMARK_GFM_EXTENSION_API_H
#define CMARK_GFM_EXTENSION_API_H

#ifdef __cplusplus
extern "C" {
#endif

#include "cmark-gfm.h"

struct cmark_renderer;
struct cmark_html_renderer;
struct cmark_chunk;

/**
 * ## Extension Support
 *
 * While the "core" of libcmark is strictly compliant with the
 * specification, an API is provided for extension writers to
 * hook into the parsing process.
 *
 * It should be noted that the cmark_node API already offers
 * room for customization, with methods offered to traverse and
 * modify the AST, and even define custom blocks.
 * When the desired customization is achievable in an error-proof
 * way using that API, it should be the preferred method.
 *
 * The following API requires a more in-depth understanding
 * of libcmark's parsing strategy, which is exposed
 * [here](http://spec.commonmark.org/0.24/#appendix-a-parsing-strategy).
 *
 * It should be used when "a posteriori" modification of the AST
 * proves to be too difficult / impossible to implement correctly.
 *
 * It can also serve as an intermediary step before extending
 * the specification, as an extension implemented using this API
 * will be trivially integrated in the core if it proves to be
 * desirable.
 */

typedef struct cmark_plugin cmark_plugin;

/** A syntax extension that can be attached to a cmark_parser
 * with cmark_parser_attach_syntax_extension().
 *
 * Extension writers should assign functions matching
 * the signature of the following 'virtual methods' to
 * implement new functionality.
 *
 * Their calling order and expected behaviour match the procedure outlined
 * at <http://spec.commonmark.org/0.24/#phase-1-block-structure>:
 *
 * During step 1, cmark will call the function provided through
 * 'cmark_syntax_extension_set_match_block_func' when it
 * iterates over an open block created by this extension,
 * to determine  whether it could contain the new line.
 * If no function was provided, cmark will close the block.
 *
 * During step 2, if and only if the new line doesn't match any
 * of the standard syntax rules, cmark will call the function
 * provided through 'cmark_syntax_extension_set_open_block_func'
 * to let the extension determine whether that new line matches
 * one of its syntax rules.
 * It is the responsibility of the parser to create and add the
 * new block with cmark_parser_make_block and cmark_parser_add_child.
 * If no function was provided is NULL, the extension will have
 * no effect at all on the final block structure of the AST.
 *
 * #### Inline parsing phase hooks
 *
 * For each character provided by the extension through
 * 'cmark_syntax_extension_set_special_inline_chars',
 * the function provided by the extension through
 * 'cmark_syntax_extension_set_match_inline_func'
 * will get called, it is the responsibility of the extension
 * to scan the characters located at the current inline parsing offset
 * with the cmark_inline_parser API.
 *
 * Depending on the type of the extension, it can either:
 *
 * * Scan forward, determine that the syntax matches and return
 *   a newly-created inline node with the appropriate type.
 *   This is the technique that would be used if inline code
 *   (with backticks) was implemented as an extension.
 * * Scan only the character(s) that its syntax rules require
 *   for opening and closing nodes, push a delimiter on the
 *   delimiter stack, and return a simple text node with its
 *   contents set to the character(s) consumed.
 *   This is the technique that would be used if emphasis
 *   inlines were implemented as an extension.
 *
 * When an extension has pushed delimiters on the stack,
 * the function provided through
 * 'cmark_syntax_extension_set_inline_from_delim_func'
 * will get called in a latter phase,
 * when the inline parser has matched opener and closer delimiters
 * created by the extension together.
 *
 * It is then the responsibility of the extension to modify
 * and populate the opener inline text node, and to remove
 * the necessary delimiters from the delimiter stack.
 *
 * Finally, the extension should return NULL if its scan didn't
 * match its syntax rules.
 *
 * The extension can store whatever private data it might need
 * with 'cmark_syntax_extension_set_private',
 * and optionally define a free function for this data.
 */
typedef struct subject cmark_inline_parser;

/** Exposed raw for now */

typedef struct delimiter {
  struct delimiter *previous;
  struct delimiter *next;
  cmark_node *inl_text;
  bufsize_t position;
  bufsize_t length;
  unsigned char delim_char;
  int can_open;
  int can_close;
} delimiter;

/**
 * ### Plugin API.
 *
 * Extensions should be distributed as dynamic libraries,
 * with a single exported function named after the distributed
 * filename.
 *
 * When discovering extensions (see cmark_init), cmark will
 * try to load a symbol named "init_{{filename}}" in all the
 * dynamic libraries it encounters.
 *
 * For example, given a dynamic library named myextension.so
 * (or myextension.dll), cmark will try to load the symbol
 * named "init_myextension". This means that the filename
 * must lend itself to forming a valid C identifier, with
 * the notable exception of dashes, which will be translated
 * to underscores, which means cmark will look for a function
 * named "init_my_extension" if it encounters a dynamic library
 * named "my-extension.so".
 *
 * See the 'cmark_plugin_init_func' typedef for the exact prototype
 * this function should follow.
 *
 * For now the extensibility of cmark is not complete, as
 * it only offers API to hook into the block parsing phase
 * (<http://spec.commonmark.org/0.24/#phase-1-block-structure>).
 *
 * See 'cmark_plugin_register_syntax_extension' for more information.
 */

/** The prototype plugins' init function should follow.
 */
typedef int (*cmark_plugin_init_func)(cmark_plugin *plugin);

/** Register a syntax 'extension' with the 'plugin', it will be made
 * available as an extension and, if attached to a cmark_parser
 * with 'cmark_parser_attach_syntax_extension', it will contribute
 * to the block parsing process.
 *
 * See the documentation for 'cmark_syntax_extension' for information
 * on how to implement one.
 *
 * This function will typically be called from the init function
 * of external modules.
 *
 * This takes ownership of 'extension', one should not call
 * 'cmark_syntax_extension_free' on a registered extension.
 */

int cmark_plugin_register_syntax_extension(cmark_plugin *plugin,
                                            cmark_syntax_extension *extension);

/** This will search for the syntax extension named 'name' among the
 *  registered syntax extensions.
 *
 *  It can then be attached to a cmark_parser
 *  with the cmark_parser_attach_syntax_extension method.
 */

cmark_syntax_extension *cmark_find_syntax_extension(const char *name);

/** Should create and add a new open block to 'parent_container' if
 * 'input' matches a syntax rule for that block type. It is allowed
 * to modify the type of 'parent_container'.
 *
 * Should return the newly created block if there is one, or
 * 'parent_container' if its type was modified, or NULL.
 */
typedef cmark_node * (*cmark_open_block_func) (cmark_syntax_extension *extension,
                                       int indented,
                                       cmark_parser *parser,
                                       cmark_node *parent_container,
                                       unsigned char *input,
                                       int len);

typedef cmark_node *(*cmark_match_inline_func)(cmark_syntax_extension *extension,
                                       cmark_parser *parser,
                                       cmark_node *parent,
                                       unsigned char character,
                                       cmark_inline_parser *inline_parser);

typedef delimiter *(*cmark_inline_from_delim_func)(cmark_syntax_extension *extension,
                                           cmark_parser *parser,
                                           cmark_inline_parser *inline_parser,
                                           delimiter *opener,
                                           delimiter *closer);

/** Should return 'true' if 'input' can be contained in 'container',
 *  'false' otherwise.
 */
typedef int (*cmark_match_block_func)        (cmark_syntax_extension *extension,
                                       cmark_parser *parser,
                                       unsigned char *input,
                                       int len,
                                       cmark_node *container);

typedef const char *(*cmark_get_type_string_func) (cmark_syntax_extension *extension,
                                                   cmark_node *node);

typedef int (*cmark_can_contain_func) (cmark_syntax_extension *extension,
                                       cmark_node *node,
                                       cmark_node_type child);

typedef int (*cmark_contains_inlines_func) (cmark_syntax_extension *extension,
                                            cmark_node *node);

typedef void (*cmark_common_render_func) (cmark_syntax_extension *extension,
                                          struct cmark_renderer *renderer,
                                          cmark_node *node,
                                          cmark_event_type ev_type,
                                          int options);

typedef int (*cmark_commonmark_escape_func) (cmark_syntax_extension *extension,
                                              cmark_node *node,
                                              int c);

typedef const char* (*cmark_xml_attr_func) (cmark_syntax_extension *extension,
                                            cmark_node *node);

typedef void (*cmark_html_render_func) (cmark_syntax_extension *extension,
                                        struct cmark_html_renderer *renderer,
                                        cmark_node *node,
                                        cmark_event_type ev_type,
                                        int options);

typedef int (*cmark_html_filter_func) (cmark_syntax_extension *extension,
                                       const unsigned char *tag,
                                       size_t tag_len);

typedef cmark_node *(*cmark_postprocess_func) (cmark_syntax_extension *extension,
                                               cmark_parser *parser,
                                               cmark_node *root);

typedef int (*cmark_ispunct_func) (char c);

typedef void (*cmark_opaque_alloc_func) (cmark_syntax_extension *extension,
                                         cmark_mem *mem,
                                         cmark_node *node);

typedef void (*cmark_opaque_free_func) (cmark_syntax_extension *extension,
                                        cmark_mem *mem,
                                        cmark_node *node);

/** Free a cmark_syntax_extension.
 */

void cmark_syntax_extension_free               (cmark_mem *mem, cmark_syntax_extension *extension);

/** Return a newly-constructed cmark_syntax_extension, named 'name'.
 */

cmark_syntax_extension *cmark_syntax_extension_new (const char *name);


cmark_node_type cmark_syntax_extension_add_node(int is_inline);


void cmark_syntax_extension_set_emphasis(cmark_syntax_extension *extension, int emphasis);

/** See the documentation for 'cmark_syntax_extension'
 */

void cmark_syntax_extension_set_open_block_func(cmark_syntax_extension *extension,
                                                cmark_open_block_func func);

/** See the documentation for 'cmark_syntax_extension'
 */

void cmark_syntax_extension_set_match_block_func(cmark_syntax_extension *extension,
                                                 cmark_match_block_func func);

/** See the documentation for 'cmark_syntax_extension'
 */

void cmark_syntax_extension_set_match_inline_func(cmark_syntax_extension *extension,
                                                  cmark_match_inline_func func);

/** See the documentation for 'cmark_syntax_extension'
 */

void cmark_syntax_extension_set_inline_from_delim_func(cmark_syntax_extension *extension,
                                                       cmark_inline_from_delim_func func);

/** See the documentation for 'cmark_syntax_extension'
 */

void cmark_syntax_extension_set_special_inline_chars(cmark_syntax_extension *extension,
                                                     cmark_llist *special_chars);

/** See the documentation for 'cmark_syntax_extension'
 */

void cmark_syntax_extension_set_get_type_string_func(cmark_syntax_extension *extension,
                                                     cmark_get_type_string_func func);

/** See the documentation for 'cmark_syntax_extension'
 */

void cmark_syntax_extension_set_can_contain_func(cmark_syntax_extension *extension,
                                                 cmark_can_contain_func func);

/** See the documentation for 'cmark_syntax_extension'
 */

void cmark_syntax_extension_set_contains_inlines_func(cmark_syntax_extension *extension,
                                                      cmark_contains_inlines_func func);

/** See the documentation for 'cmark_syntax_extension'
 */

void cmark_syntax_extension_set_commonmark_render_func(cmark_syntax_extension *extension,
                                                       cmark_common_render_func func);

/** See the documentation for 'cmark_syntax_extension'
 */

void cmark_syntax_extension_set_plaintext_render_func(cmark_syntax_extension *extension,
                                                      cmark_common_render_func func);

/** See the documentation for 'cmark_syntax_extension'
 */

void cmark_syntax_extension_set_latex_render_func(cmark_syntax_extension *extension,
                                                  cmark_common_render_func func);

/** See the documentation for 'cmark_syntax_extension'
 */

void cmark_syntax_extension_set_xml_attr_func(cmark_syntax_extension *extension,
                                              cmark_xml_attr_func func);

  /** See the documentation for 'cmark_syntax_extension'
 */

void cmark_syntax_extension_set_man_render_func(cmark_syntax_extension *extension,
                                                cmark_common_render_func func);

/** See the documentation for 'cmark_syntax_extension'
 */

void cmark_syntax_extension_set_html_render_func(cmark_syntax_extension *extension,
                                                 cmark_html_render_func func);

/** See the documentation for 'cmark_syntax_extension'
 */

void cmark_syntax_extension_set_html_filter_func(cmark_syntax_extension *extension,
                                                 cmark_html_filter_func func);

/** See the documentation for 'cmark_syntax_extension'
 */

void cmark_syntax_extension_set_commonmark_escape_func(cmark_syntax_extension *extension,
                                                       cmark_commonmark_escape_func func);

/** See the documentation for 'cmark_syntax_extension'
 */

void cmark_syntax_extension_set_private(cmark_syntax_extension *extension,
                                        void *priv,
                                        cmark_free_func free_func);

/** See the documentation for 'cmark_syntax_extension'
 */

void *cmark_syntax_extension_get_private(cmark_syntax_extension *extension);

/** See the documentation for 'cmark_syntax_extension'
 */

void cmark_syntax_extension_set_postprocess_func(cmark_syntax_extension *extension,
                                                 cmark_postprocess_func func);

/** See the documentation for 'cmark_syntax_extension'
 */

void cmark_syntax_extension_set_opaque_alloc_func(cmark_syntax_extension *extension,
                                                  cmark_opaque_alloc_func func);

/** See the documentation for 'cmark_syntax_extension'
 */

void cmark_syntax_extension_set_opaque_free_func(cmark_syntax_extension *extension,
                                                 cmark_opaque_free_func func);

/** See the documentation for 'cmark_syntax_extension'
 */

void cmark_parser_set_backslash_ispunct_func(cmark_parser *parser,
                                             cmark_ispunct_func func);

/** Return the index of the line currently being parsed, starting with 1.
 */

int cmark_parser_get_line_number(cmark_parser *parser);

/** Return the offset in bytes in the line being processed.
 *
 * Example:
 *
 * ### foo
 *
 * Here, offset will first be 0, then 5 (the index of the 'f' character).
 */

int cmark_parser_get_offset(cmark_parser *parser);

/**
 * Return the offset in 'columns' in the line being processed.
 *
 * This value may differ from the value returned by
 * cmark_parser_get_offset() in that it accounts for tabs,
 * and as such should not be used as an index in the current line's
 * buffer.
 *
 * Example:
 *
 * cmark_parser_advance_offset() can be called to advance the
 * offset by a number of columns, instead of a number of bytes.
 *
 * In that case, if offset falls "in the middle" of a tab
 * character, 'column' and offset will differ.
 *
 * ```
 * foo                 \t bar
 * ^                   ^^
 * offset (0)          20
 * ```
 *
 * If cmark_parser_advance_offset is called here with 'columns'
 * set to 'true' and 'offset' set to 22, cmark_parser_get_offset()
 * will return 20, whereas cmark_parser_get_column() will return
 * 22.
 *
 * Additionally, as tabs expand to the next multiple of 4 column,
 * cmark_parser_has_partially_consumed_tab() will now return
 * 'true'.
 */

int cmark_parser_get_column(cmark_parser *parser);

/** Return the absolute index in bytes of the first nonspace
 * character coming after the offset as returned by
 * cmark_parser_get_offset() in the line currently being processed.
 *
 * Example:
 *
 * ```
 *   foo        bar            baz  \n
 * ^               ^           ^
 * 0            offset (16) first_nonspace (28)
 * ```
 */

int cmark_parser_get_first_nonspace(cmark_parser *parser);

/** Return the absolute index of the first nonspace column coming after 'offset'
 * in the line currently being processed, counting tabs as multiple
 * columns as appropriate.
 *
 * See the documentation for cmark_parser_get_first_nonspace() and
 * cmark_parser_get_column() for more information.
 */

int cmark_parser_get_first_nonspace_column(cmark_parser *parser);

/** Return the difference between the values returned by
 * cmark_parser_get_first_nonspace_column() and
 * cmark_parser_get_column().
 *
 * This is not a byte offset, as it can count one tab as multiple
 * characters.
 */

int cmark_parser_get_indent(cmark_parser *parser);

/** Return 'true' if the line currently being processed has been entirely
 * consumed, 'false' otherwise.
 *
 * Example:
 *
 * ```
 *   foo        bar            baz  \n
 * ^
 * offset
 * ```
 *
 * This function will return 'false' here.
 *
 * ```
 *   foo        bar            baz  \n
 *                 ^
 *              offset
 * ```
 * This function will still return 'false'.
 *
 * ```
 *   foo        bar            baz  \n
 *                                ^
 *                             offset
 * ```
 *
 * At this point, this function will now return 'true'.
 */

int cmark_parser_is_blank(cmark_parser *parser);

/** Return 'true' if the value returned by cmark_parser_get_offset()
 * is 'inside' an expanded tab.
 *
 * See the documentation for cmark_parser_get_column() for more
 * information.
 */

int cmark_parser_has_partially_consumed_tab(cmark_parser *parser);

/** Return the length in bytes of the previously processed line, excluding potential
 * newline (\n) and carriage return (\r) trailing characters.
 */

int cmark_parser_get_last_line_length(cmark_parser *parser);

/** Add a child to 'parent' during the parsing process.
 *
 * If 'parent' isn't the kind of node that can accept this child,
 * this function will back up till it hits a node that can, closing
 * blocks as appropriate.
 */

cmark_node*cmark_parser_add_child(cmark_parser *parser,
                                  cmark_node *parent,
                                  cmark_node_type block_type,
                                  int start_column);

/** Advance the 'offset' of the parser in the current line.
 *
 * See the documentation of cmark_parser_get_offset() and
 * cmark_parser_get_column() for more information.
 */

void cmark_parser_advance_offset(cmark_parser *parser,
                                 const char *input,
                                 int count,
                                 int columns);



void cmark_parser_feed_reentrant(cmark_parser *parser, const char *buffer, size_t len);

/** Attach the syntax 'extension' to the 'parser', to provide extra syntax
 *  rules.
 *  See the documentation for cmark_syntax_extension for more information.
 *
 *  Returns 'true' if the 'extension' was successfully attached,
 *  'false' otherwise.
 */

int cmark_parser_attach_syntax_extension(cmark_parser *parser, cmark_syntax_extension *extension);

/** Change the type of 'node'.
 *
 * Return 0 if the type could be changed, 1 otherwise.
 */
 int cmark_node_set_type(cmark_node *node, cmark_node_type type);

/** Return the string content for all types of 'node'.
 *  The pointer stays valid as long as 'node' isn't freed.
 */
 const char *cmark_node_get_string_content(cmark_node *node);

/** Set the string 'content' for all types of 'node'.
 *  Copies 'content'.
 */
 int cmark_node_set_string_content(cmark_node *node, const char *content);

/** Get the syntax extension responsible for the creation of 'node'.
 *  Return NULL if 'node' was created because it matched standard syntax rules.
 */
 cmark_syntax_extension *cmark_node_get_syntax_extension(cmark_node *node);

/** Set the syntax extension responsible for creating 'node'.
 */
 int cmark_node_set_syntax_extension(cmark_node *node,
                                                  cmark_syntax_extension *extension);

/**
 * ## Inline syntax extension helpers
 *
 * The inline parsing process is described in detail at
 * <http://spec.commonmark.org/0.24/#phase-2-inline-structure>
 */

/** Should return 'true' if the predicate matches 'c', 'false' otherwise
 */
typedef int (*cmark_inline_predicate)(int c);

/** Advance the current inline parsing offset */

void cmark_inline_parser_advance_offset(cmark_inline_parser *parser);

/** Get the current inline parsing offset */

int cmark_inline_parser_get_offset(cmark_inline_parser *parser);

/** Set the offset in bytes in the chunk being processed by the given inline parser.
 */

void cmark_inline_parser_set_offset(cmark_inline_parser *parser, int offset);

/** Gets the cmark_chunk being operated on by the given inline parser.
 * Use cmark_inline_parser_get_offset to get our current position in the chunk.
 */

struct cmark_chunk *cmark_inline_parser_get_chunk(cmark_inline_parser *parser);

/** Returns 1 if the inline parser is currently in a bracket; pass 1 for 'image'
 * if you want to know about an image-type bracket, 0 for link-type. */

int cmark_inline_parser_in_bracket(cmark_inline_parser *parser, int image);

/** Remove the last n characters from the last child of the given node.
 * This only works where all n characters are in the single last child, and the last
 * child is CMARK_NODE_TEXT.
 */

void cmark_node_unput(cmark_node *node, int n);


/** Get the character located at the current inline parsing offset
 */

unsigned char cmark_inline_parser_peek_char(cmark_inline_parser *parser);

/** Get the character located 'pos' bytes in the current line.
 */

unsigned char cmark_inline_parser_peek_at(cmark_inline_parser *parser, int pos);

/** Whether the inline parser has reached the end of the current line
 */

int cmark_inline_parser_is_eof(cmark_inline_parser *parser);

/** Get the characters located after the current inline parsing offset
 * while 'pred' matches. Free after usage.
 */

char *cmark_inline_parser_take_while(cmark_inline_parser *parser, cmark_inline_predicate pred);

/** Push a delimiter on the delimiter stack.
 * See <<http://spec.commonmark.org/0.24/#phase-2-inline-structure> for
 * more information on the parameters
 */

void cmark_inline_parser_push_delimiter(cmark_inline_parser *parser,
                                  unsigned char c,
                                  int can_open,
                                  int can_close,
                                  cmark_node *inl_text);

/** Remove 'delim' from the delimiter stack
 */

void cmark_inline_parser_remove_delimiter(cmark_inline_parser *parser, delimiter *delim);


delimiter *cmark_inline_parser_get_last_delimiter(cmark_inline_parser *parser);


int cmark_inline_parser_get_line(cmark_inline_parser *parser);


int cmark_inline_parser_get_column(cmark_inline_parser *parser);

/** Convenience function to scan a given delimiter.
 *
 * 'left_flanking' and 'right_flanking' will be set to true if they
 * respectively precede and follow a non-space, non-punctuation
 * character.
 *
 * Additionally, 'punct_before' and 'punct_after' will respectively be set
 * if the preceding or following character is a punctuation character.
 *
 * Note that 'left_flanking' and 'right_flanking' can both be 'true'.
 *
 * Returns the number of delimiters encountered, in the limit
 * of 'max_delims', and advances the inline parsing offset.
 */

int cmark_inline_parser_scan_delimiters(cmark_inline_parser *parser,
                                  int max_delims,
                                  unsigned char c,
                                  int *left_flanking,
                                  int *right_flanking,
                                  int *punct_before,
                                  int *punct_after);


void cmark_manage_extensions_special_characters(cmark_parser *parser, int add);


cmark_llist *cmark_parser_get_syntax_extensions(cmark_parser *parser);


void cmark_arena_push(void);


int cmark_arena_pop(void);

#ifdef __cplusplus
}
#endif

#endif
