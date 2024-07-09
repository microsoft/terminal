#include "cmark-gfm-extension_api.h"
#include "html.h"
#include "inlines.h"
#include "parser.h"
#include "references.h"
#include <string.h>
#include "render.h"

#include "ext_scanners.h"
#include "strikethrough.h"
#include "table.h"
#include "cmark-gfm-core-extensions.h"

// Limit to prevent a malicious input from causing a denial of service.
#define MAX_AUTOCOMPLETED_CELLS 0x80000

// Custom node flag, initialized in `create_table_extension`.
static cmark_node_internal_flags CMARK_NODE__TABLE_VISITED;

cmark_node_type CMARK_NODE_TABLE, CMARK_NODE_TABLE_ROW,
    CMARK_NODE_TABLE_CELL;

typedef struct {
  cmark_strbuf *buf;
  int start_offset, end_offset, internal_offset;
} node_cell;

typedef struct {
  uint16_t n_columns;
  int paragraph_offset;
  node_cell *cells;
} table_row;

typedef struct {
  uint16_t n_columns;
  uint8_t *alignments;
  int n_rows;
  int n_nonempty_cells;
} node_table;

typedef struct {
  bool is_header;
} node_table_row;

static void free_table_cell(cmark_mem *mem, node_cell *cell) {
  cmark_strbuf_free((cmark_strbuf *)cell->buf);
  mem->free(cell->buf);
}

static void free_row_cells(cmark_mem *mem, table_row *row) {
  while (row->n_columns > 0) {
    free_table_cell(mem, &row->cells[--row->n_columns]);
  }
  mem->free(row->cells);
  row->cells = NULL;
}

static void free_table_row(cmark_mem *mem, table_row *row) {
  if (!row)
    return;

  free_row_cells(mem, row);
  mem->free(row);
}

static void free_node_table(cmark_mem *mem, void *ptr) {
  node_table *t = (node_table *)ptr;
  mem->free(t->alignments);
  mem->free(t);
}

static void free_node_table_row(cmark_mem *mem, void *ptr) {
  mem->free(ptr);
}

static int get_n_table_columns(cmark_node *node) {
  if (!node || node->type != CMARK_NODE_TABLE)
    return -1;

  return (int)((node_table *)node->as.opaque)->n_columns;
}

static int set_n_table_columns(cmark_node *node, uint16_t n_columns) {
  if (!node || node->type != CMARK_NODE_TABLE)
    return 0;

  ((node_table *)node->as.opaque)->n_columns = n_columns;
  return 1;
}

// Increment the number of rows in the table. Also update n_nonempty_cells,
// which keeps track of the number of cells which were parsed from the
// input file. (If one of the rows is too short, then the trailing cells
// are autocompleted. Autocompleted cells are not counted in n_nonempty_cells.)
// The purpose of this is to prevent a malicious input from generating a very
// large number of autocompleted cells, which could cause a denial of service
// vulnerability.
static int incr_table_row_count(cmark_node *node, int i) {
  if (!node || node->type != CMARK_NODE_TABLE) {
    return 0;
  }

  ((node_table *)node->as.opaque)->n_rows++;
  ((node_table *)node->as.opaque)->n_nonempty_cells += i;
  return 1;
}

// Calculate the number of autocompleted cells.
static int get_n_autocompleted_cells(cmark_node *node) {
  if (!node || node->type != CMARK_NODE_TABLE) {
    return 0;
  }

  const node_table *nt = (node_table *)node->as.opaque;
  return (nt->n_columns * nt->n_rows) - nt->n_nonempty_cells;
}

static uint8_t *get_table_alignments(cmark_node *node) {
  if (!node || node->type != CMARK_NODE_TABLE)
    return 0;

  return ((node_table *)node->as.opaque)->alignments;
}

static int set_table_alignments(cmark_node *node, uint8_t *alignments) {
  if (!node || node->type != CMARK_NODE_TABLE)
    return 0;

  ((node_table *)node->as.opaque)->alignments = alignments;
  return 1;
}

static uint8_t get_cell_alignment(cmark_node *node) {
  if (!node || node->type != CMARK_NODE_TABLE_CELL)
    return 0;

  const uint8_t *alignments = get_table_alignments(node->parent->parent);
  int i = node->as.cell_index;
  return alignments[i];
}

static int set_cell_index(cmark_node *node, int i) {
  if (!node || node->type != CMARK_NODE_TABLE_CELL)
    return 0;

  node->as.cell_index = i;
  return 1;
}

static cmark_strbuf *unescape_pipes(cmark_mem *mem, unsigned char *string, bufsize_t len)
{
  cmark_strbuf *res = (cmark_strbuf *)mem->calloc(1, sizeof(cmark_strbuf));
  bufsize_t r, w;

  cmark_strbuf_init(mem, res, len + 1);
  cmark_strbuf_put(res, string, len);
  cmark_strbuf_putc(res, '\0');

  for (r = 0, w = 0; r < len; ++r) {
    if (res->ptr[r] == '\\' && res->ptr[r + 1] == '|')
      r++;

    res->ptr[w++] = res->ptr[r];
  }

  cmark_strbuf_truncate(res, w);

  return res;
}

// Adds a new cell to the end of the row. A pointer to the new cell is returned
// for the caller to initialize.
static node_cell* append_row_cell(cmark_mem *mem, table_row *row) {
  const uint32_t n_columns = row->n_columns + 1;
  // realloc when n_columns is a power of 2
  if ((n_columns & (n_columns-1)) == 0) {
    // make sure we never wrap row->n_columns
    // offset will != len and our exit will clean up as intended
    if (n_columns > UINT16_MAX) {
      return NULL;
    }
    // Use realloc to double the size of the buffer.
    row->cells = (node_cell *)mem->realloc(row->cells, (2 * n_columns - 1) * sizeof(node_cell));
  }
  row->n_columns = (uint16_t)n_columns;
  return &row->cells[n_columns-1];
}

static table_row *row_from_string(cmark_syntax_extension *self,
                                  cmark_parser *parser, unsigned char *string,
                                  int len) {
  // Parses a single table row. It has the following form:
  // `delim? table_cell (delim table_cell)* delim? newline`
  // Note that cells are allowed to be empty.
  //
  // From the GitHub-flavored Markdown specification:
  //
  // > Each row consists of cells containing arbitrary text, in which inlines
  // > are parsed, separated by pipes (|). A leading and trailing pipe is also
  // > recommended for clarity of reading, and if there’s otherwise parsing
  // > ambiguity.

  table_row *row = NULL;
  bufsize_t cell_matched = 1, pipe_matched = 1, offset;
  int expect_more_cells = 1;
  int row_end_offset = 0;
  int int_overflow_abort = 0;

  row = (table_row *)parser->mem->calloc(1, sizeof(table_row));
  row->n_columns = 0;
  row->cells = NULL;

  // Scan past the (optional) leading pipe.
  offset = scan_table_cell_end(string, len, 0);

  // Parse the cells of the row. Stop if we reach the end of the input, or if we
  // cannot detect any more cells.
  while (offset < len && expect_more_cells) {
    cell_matched = scan_table_cell(string, len, offset);
    pipe_matched = scan_table_cell_end(string, len, offset + cell_matched);

    if (cell_matched || pipe_matched) {
      // We are guaranteed to have a cell, since (1) either we found some
      // content and cell_matched, or (2) we found an empty cell followed by a
      // pipe.
      cmark_strbuf *cell_buf = unescape_pipes(parser->mem, string + offset,
          cell_matched);
      cmark_strbuf_trim(cell_buf);

      node_cell *cell = append_row_cell(parser->mem, row);
      if (!cell) {
        int_overflow_abort = 1;
        cmark_strbuf_free(cell_buf);
        parser->mem->free(cell_buf);
        break;
      }
      cell->buf = cell_buf;
      cell->start_offset = offset;
      cell->end_offset = offset + cell_matched - 1;
      cell->internal_offset = 0;

      while (cell->start_offset > row->paragraph_offset && string[cell->start_offset - 1] != '|') {
        --cell->start_offset;
        ++cell->internal_offset;
      }
    }

    offset += cell_matched + pipe_matched;

    if (pipe_matched) {
      expect_more_cells = 1;
    } else {
      // We've scanned the last cell. Check if we have reached the end of the row
      row_end_offset = scan_table_row_end(string, len, offset);
      offset += row_end_offset;

      // If the end of the row is not the end of the input,
      // the row is not a real row but potentially part of the paragraph
      // preceding the table.
      if (row_end_offset && offset != len) {
        row->paragraph_offset = offset;

        free_row_cells(parser->mem, row);

        // Scan past the (optional) leading pipe.
        offset += scan_table_cell_end(string, len, offset);

        expect_more_cells = 1;
      } else {
        expect_more_cells = 0;
      }
    }
  }

  if (offset != len || row->n_columns == 0 || int_overflow_abort) {
    free_table_row(parser->mem, row);
    row = NULL;
  }

  return row;
}

static void try_inserting_table_header_paragraph(cmark_parser *parser,
                                                 cmark_node *parent_container,
                                                 unsigned char *parent_string,
                                                 int paragraph_offset) {
  cmark_node *paragraph;
  cmark_strbuf *paragraph_content;

  paragraph = cmark_node_new_with_mem(CMARK_NODE_PARAGRAPH, parser->mem);

  paragraph_content = unescape_pipes(parser->mem, parent_string, paragraph_offset);
  cmark_strbuf_trim(paragraph_content);
  cmark_node_set_string_content(paragraph, (char *) paragraph_content->ptr);
  cmark_strbuf_free(paragraph_content);
  parser->mem->free(paragraph_content);

  if (!cmark_node_insert_before(parent_container, paragraph)) {
    parser->mem->free(paragraph);
  }
}

static cmark_node *try_opening_table_header(cmark_syntax_extension *self,
                                            cmark_parser *parser,
                                            cmark_node *parent_container,
                                            unsigned char *input, int len) {
  cmark_node *table_header;
  table_row *header_row = NULL;
  table_row *delimiter_row = NULL;
  node_table_row *ntr;
  const char *parent_string;
  uint16_t i;

  if (parent_container->flags & CMARK_NODE__TABLE_VISITED) {
    return parent_container;
  }

  if (!scan_table_start(input, len, cmark_parser_get_first_nonspace(parser))) {
    return parent_container;
  }

  // Since scan_table_start was successful, we must have a delimiter row.
  delimiter_row = row_from_string(
    self, parser, input + cmark_parser_get_first_nonspace(parser),
    len - cmark_parser_get_first_nonspace(parser));
  // assert may be optimized out, don't rely on it for security boundaries
  if (!delimiter_row) {
      return parent_container;
  }

  assert(delimiter_row);

  cmark_arena_push();

  // Check for a matching header row. We call `row_from_string` with the entire
  // (potentially long) parent container as input, but this should be safe since
  // `row_from_string` bails out early if it does not find a row.
  parent_string = cmark_node_get_string_content(parent_container);
  header_row = row_from_string(self, parser, (unsigned char *)parent_string,
                               (int)strlen(parent_string));
  if (!header_row || header_row->n_columns != delimiter_row->n_columns) {
    free_table_row(parser->mem, delimiter_row);
    free_table_row(parser->mem, header_row);
    cmark_arena_pop();
    parent_container->flags |= CMARK_NODE__TABLE_VISITED;
    return parent_container;
  }

  if (cmark_arena_pop()) {
    delimiter_row = row_from_string(
        self, parser, input + cmark_parser_get_first_nonspace(parser),
        len - cmark_parser_get_first_nonspace(parser));
    header_row = row_from_string(self, parser, (unsigned char *)parent_string,
                                 (int)strlen(parent_string));
    // row_from_string can return NULL, add additional check to ensure n_columns match
    if (!delimiter_row || !header_row || header_row->n_columns != delimiter_row->n_columns) {
        free_table_row(parser->mem, delimiter_row);
        free_table_row(parser->mem, header_row);
        return parent_container;
    }
  }

  if (!cmark_node_set_type(parent_container, CMARK_NODE_TABLE)) {
    free_table_row(parser->mem, header_row);
    free_table_row(parser->mem, delimiter_row);
    return parent_container;
  }

  if (header_row->paragraph_offset) {
    try_inserting_table_header_paragraph(parser, parent_container, (unsigned char *)parent_string,
                                         header_row->paragraph_offset);
  }

  cmark_node_set_syntax_extension(parent_container, self);
  parent_container->as.opaque = parser->mem->calloc(1, sizeof(node_table));
  set_n_table_columns(parent_container, header_row->n_columns);

  // allocate alignments based on delimiter_row->n_columns
  // since we populate the alignments array based on delimiter_row->cells
  uint8_t *alignments =
      (uint8_t *)parser->mem->calloc(delimiter_row->n_columns, sizeof(uint8_t));
  for (i = 0; i < delimiter_row->n_columns; ++i) {
    node_cell *node = &delimiter_row->cells[i];
    bool left = node->buf->ptr[0] == ':', right = node->buf->ptr[node->buf->size - 1] == ':';

    if (left && right)
      alignments[i] = 'c';
    else if (left)
      alignments[i] = 'l';
    else if (right)
      alignments[i] = 'r';
  }
  set_table_alignments(parent_container, alignments);

  table_header =
      cmark_parser_add_child(parser, parent_container, CMARK_NODE_TABLE_ROW,
                             parent_container->start_column);
  cmark_node_set_syntax_extension(table_header, self);
  table_header->end_column = parent_container->start_column + (int)strlen(parent_string) - 2;
  table_header->start_line = table_header->end_line = parent_container->start_line;

  table_header->as.opaque = ntr = (node_table_row *)parser->mem->calloc(1, sizeof(node_table_row));
  ntr->is_header = true;

  for (i = 0; i < header_row->n_columns; ++i) {
    node_cell *cell = &header_row->cells[i];
    cmark_node *header_cell = cmark_parser_add_child(parser, table_header,
                                                     CMARK_NODE_TABLE_CELL, parent_container->start_column + cell->start_offset);
    header_cell->start_line = header_cell->end_line = parent_container->start_line;
    header_cell->internal_offset = cell->internal_offset;
    header_cell->end_column = parent_container->start_column + cell->end_offset;
    cmark_node_set_string_content(header_cell, (char *) cell->buf->ptr);
    cmark_node_set_syntax_extension(header_cell, self);
    set_cell_index(header_cell, i);
  }

  incr_table_row_count(parent_container, i);

  cmark_parser_advance_offset(
      parser, (char *)input,
      (int)strlen((char *)input) - 1 - cmark_parser_get_offset(parser), false);

  free_table_row(parser->mem, header_row);
  free_table_row(parser->mem, delimiter_row);
  return parent_container;
}

static cmark_node *try_opening_table_row(cmark_syntax_extension *self,
                                         cmark_parser *parser,
                                         cmark_node *parent_container,
                                         unsigned char *input, int len) {
  cmark_node *table_row_block;
  table_row *row;

  if (cmark_parser_is_blank(parser))
    return NULL;

  if (get_n_autocompleted_cells(parent_container) > MAX_AUTOCOMPLETED_CELLS) {
    return NULL;
  }

  table_row_block =
      cmark_parser_add_child(parser, parent_container, CMARK_NODE_TABLE_ROW,
                             parent_container->start_column);
  cmark_node_set_syntax_extension(table_row_block, self);
  table_row_block->end_column = parent_container->end_column;
  table_row_block->as.opaque = parser->mem->calloc(1, sizeof(node_table_row));

  row = row_from_string(self, parser, input + cmark_parser_get_first_nonspace(parser),
      len - cmark_parser_get_first_nonspace(parser));

  if (!row) {
      // clean up the dangling node
      cmark_node_free(table_row_block);
      return NULL;
  }

  {
    int i, table_columns = get_n_table_columns(parent_container);

    for (i = 0; i < row->n_columns && i < table_columns; ++i) {
      node_cell *cell = &row->cells[i];
      cmark_node *node = cmark_parser_add_child(parser, table_row_block,
          CMARK_NODE_TABLE_CELL, parent_container->start_column + cell->start_offset);
      node->internal_offset = cell->internal_offset;
      node->end_column = parent_container->start_column + cell->end_offset;
      cmark_node_set_string_content(node, (char *) cell->buf->ptr);
      cmark_node_set_syntax_extension(node, self);
      set_cell_index(node, i);
    }

    incr_table_row_count(parent_container, i);

    for (; i < table_columns; ++i) {
      cmark_node *node = cmark_parser_add_child(
          parser, table_row_block, CMARK_NODE_TABLE_CELL, 0);
      cmark_node_set_syntax_extension(node, self);
      set_cell_index(node, i);
    }
  }

  free_table_row(parser->mem, row);

  cmark_parser_advance_offset(parser, (char *)input,
                              len - 1 - cmark_parser_get_offset(parser), false);

  return table_row_block;
}

static cmark_node *try_opening_table_block(cmark_syntax_extension *self,
                                           int indented, cmark_parser *parser,
                                           cmark_node *parent_container,
                                           unsigned char *input, int len) {
  cmark_node_type parent_type = cmark_node_get_type(parent_container);

  if (!indented && parent_type == CMARK_NODE_PARAGRAPH) {
    return try_opening_table_header(self, parser, parent_container, input, len);
  } else if (!indented && parent_type == CMARK_NODE_TABLE) {
    return try_opening_table_row(self, parser, parent_container, input, len);
  }

  return NULL;
}

static int matches(cmark_syntax_extension *self, cmark_parser *parser,
                   unsigned char *input, int len,
                   cmark_node *parent_container) {
  int res = 0;

  if (cmark_node_get_type(parent_container) == CMARK_NODE_TABLE) {
    cmark_arena_push();
    table_row *new_row = row_from_string(
        self, parser, input + cmark_parser_get_first_nonspace(parser),
        len - cmark_parser_get_first_nonspace(parser));
    if (new_row && new_row->n_columns)
      res = 1;
    free_table_row(parser->mem, new_row);
    cmark_arena_pop();
  }

  return res;
}

static const char *get_type_string(cmark_syntax_extension *self,
                                   cmark_node *node) {
  if (node->type == CMARK_NODE_TABLE) {
    return "table";
  } else if (node->type == CMARK_NODE_TABLE_ROW) {
    if (((node_table_row *)node->as.opaque)->is_header)
      return "table_header";
    else
      return "table_row";
  } else if (node->type == CMARK_NODE_TABLE_CELL) {
    return "table_cell";
  }

  return "<unknown>";
}

static int can_contain(cmark_syntax_extension *extension, cmark_node *node,
                       cmark_node_type child_type) {
  if (node->type == CMARK_NODE_TABLE) {
    return child_type == CMARK_NODE_TABLE_ROW;
  } else if (node->type == CMARK_NODE_TABLE_ROW) {
    return child_type == CMARK_NODE_TABLE_CELL;
  } else if (node->type == CMARK_NODE_TABLE_CELL) {
    return child_type == CMARK_NODE_TEXT || child_type == CMARK_NODE_CODE ||
           child_type == CMARK_NODE_EMPH || child_type == CMARK_NODE_STRONG ||
           child_type == CMARK_NODE_LINK || child_type == CMARK_NODE_IMAGE ||
           child_type == CMARK_NODE_STRIKETHROUGH ||
           child_type == CMARK_NODE_HTML_INLINE ||
           child_type == CMARK_NODE_FOOTNOTE_REFERENCE;
  }
  return false;
}

static int contains_inlines(cmark_syntax_extension *extension,
                            cmark_node *node) {
  return node->type == CMARK_NODE_TABLE_CELL;
}

static void commonmark_render(cmark_syntax_extension *extension,
                              cmark_renderer *renderer, cmark_node *node,
                              cmark_event_type ev_type, int options) {
  bool entering = (ev_type == CMARK_EVENT_ENTER);

  if (node->type == CMARK_NODE_TABLE) {
    renderer->blankline(renderer);
  } else if (node->type == CMARK_NODE_TABLE_ROW) {
    if (entering) {
      renderer->cr(renderer);
      renderer->out(renderer, node, "|", false, LITERAL);
    }
  } else if (node->type == CMARK_NODE_TABLE_CELL) {
    if (entering) {
      renderer->out(renderer, node, " ", false, LITERAL);
    } else {
      renderer->out(renderer, node, " |", false, LITERAL);
      if (((node_table_row *)node->parent->as.opaque)->is_header &&
          !node->next) {
        int i;
        uint8_t *alignments = get_table_alignments(node->parent->parent);
        uint16_t n_cols =
            ((node_table *)node->parent->parent->as.opaque)->n_columns;
        renderer->cr(renderer);
        renderer->out(renderer, node, "|", false, LITERAL);
        for (i = 0; i < n_cols; i++) {
          switch (alignments[i]) {
          case 0:   renderer->out(renderer, node, " --- |", false, LITERAL); break;
          case 'l': renderer->out(renderer, node, " :-- |", false, LITERAL); break;
          case 'c': renderer->out(renderer, node, " :-: |", false, LITERAL); break;
          case 'r': renderer->out(renderer, node, " --: |", false, LITERAL); break;
          }
        }
        renderer->cr(renderer);
      }
    }
  } else {
    assert(false);
  }
}

static void latex_render(cmark_syntax_extension *extension,
                         cmark_renderer *renderer, cmark_node *node,
                         cmark_event_type ev_type, int options) {
  bool entering = (ev_type == CMARK_EVENT_ENTER);

  if (node->type == CMARK_NODE_TABLE) {
    if (entering) {
      int i;
      uint16_t n_cols;
      uint8_t *alignments = get_table_alignments(node);

      renderer->cr(renderer);
      renderer->out(renderer, node, "\\begin{table}", false, LITERAL);
      renderer->cr(renderer);
      renderer->out(renderer, node, "\\begin{tabular}{", false, LITERAL);

      n_cols = ((node_table *)node->as.opaque)->n_columns;
      for (i = 0; i < n_cols; i++) {
        switch(alignments[i]) {
        case 0:
        case 'l':
          renderer->out(renderer, node, "l", false, LITERAL);
          break;
        case 'c':
          renderer->out(renderer, node, "c", false, LITERAL);
          break;
        case 'r':
          renderer->out(renderer, node, "r", false, LITERAL);
          break;
        }
      }
      renderer->out(renderer, node, "}", false, LITERAL);
      renderer->cr(renderer);
    } else {
      renderer->out(renderer, node, "\\end{tabular}", false, LITERAL);
      renderer->cr(renderer);
      renderer->out(renderer, node, "\\end{table}", false, LITERAL);
      renderer->cr(renderer);
    }
  } else if (node->type == CMARK_NODE_TABLE_ROW) {
    if (!entering) {
      renderer->cr(renderer);
    }
  } else if (node->type == CMARK_NODE_TABLE_CELL) {
    if (!entering) {
      if (node->next) {
        renderer->out(renderer, node, " & ", false, LITERAL);
      } else {
        renderer->out(renderer, node, " \\\\", false, LITERAL);
      }
    }
  } else {
    assert(false);
  }
}

static const char *xml_attr(cmark_syntax_extension *extension,
                            cmark_node *node) {
  if (node->type == CMARK_NODE_TABLE_CELL) {
    if (cmark_gfm_extensions_get_table_row_is_header(node->parent)) {
      switch (get_cell_alignment(node)) {
      case 'l': return " align=\"left\"";
      case 'c': return " align=\"center\"";
      case 'r': return " align=\"right\"";
      }
    }
  }

  return NULL;
}

static void man_render(cmark_syntax_extension *extension,
                       cmark_renderer *renderer, cmark_node *node,
                       cmark_event_type ev_type, int options) {
  bool entering = (ev_type == CMARK_EVENT_ENTER);

  if (node->type == CMARK_NODE_TABLE) {
    if (entering) {
      int i;
      uint16_t n_cols;
      uint8_t *alignments = get_table_alignments(node);

      renderer->cr(renderer);
      renderer->out(renderer, node, ".TS", false, LITERAL);
      renderer->cr(renderer);
      renderer->out(renderer, node, "tab(@);", false, LITERAL);
      renderer->cr(renderer);

      n_cols = ((node_table *)node->as.opaque)->n_columns;

      for (i = 0; i < n_cols; i++) {
        switch (alignments[i]) {
        case 'l':
          renderer->out(renderer, node, "l", false, LITERAL);
          break;
        case 0:
        case 'c':
          renderer->out(renderer, node, "c", false, LITERAL);
          break;
        case 'r':
          renderer->out(renderer, node, "r", false, LITERAL);
          break;
        }
      }

      if (n_cols) {
        renderer->out(renderer, node, ".", false, LITERAL);
        renderer->cr(renderer);
      }
    } else {
      renderer->out(renderer, node, ".TE", false, LITERAL);
      renderer->cr(renderer);
    }
  } else if (node->type == CMARK_NODE_TABLE_ROW) {
    if (!entering) {
      renderer->cr(renderer);
    }
  } else if (node->type == CMARK_NODE_TABLE_CELL) {
    if (!entering && node->next) {
      renderer->out(renderer, node, "@", false, LITERAL);
    }
  } else {
    assert(false);
  }
}

static void html_table_add_align(cmark_strbuf* html, const char* align, int options) {
  if (options & CMARK_OPT_TABLE_PREFER_STYLE_ATTRIBUTES) {
    cmark_strbuf_puts(html, " style=\"text-align: ");
    cmark_strbuf_puts(html, align);
    cmark_strbuf_puts(html, "\"");
  } else {
    cmark_strbuf_puts(html, " align=\"");
    cmark_strbuf_puts(html, align);
    cmark_strbuf_puts(html, "\"");
  }
}

struct html_table_state {
  unsigned need_closing_table_body : 1;
  unsigned in_table_header : 1;
};

static void html_render(cmark_syntax_extension *extension,
                        cmark_html_renderer *renderer, cmark_node *node,
                        cmark_event_type ev_type, int options) {
  bool entering = (ev_type == CMARK_EVENT_ENTER);
  cmark_strbuf *html = renderer->html;

  // XXX: we just monopolise renderer->opaque.
  struct html_table_state *table_state =
      (struct html_table_state *)&renderer->opaque;

  if (node->type == CMARK_NODE_TABLE) {
    if (entering) {
      cmark_html_render_cr(html);
      cmark_strbuf_puts(html, "<table");
      cmark_html_render_sourcepos(node, html, options);
      cmark_strbuf_putc(html, '>');
      table_state->need_closing_table_body = false;
    } else {
      if (table_state->need_closing_table_body) {
        cmark_html_render_cr(html);
        cmark_strbuf_puts(html, "</tbody>");
        cmark_html_render_cr(html);
      }
      table_state->need_closing_table_body = false;
      cmark_html_render_cr(html);
      cmark_strbuf_puts(html, "</table>");
      cmark_html_render_cr(html);
    }
  } else if (node->type == CMARK_NODE_TABLE_ROW) {
    if (entering) {
      cmark_html_render_cr(html);
      if (((node_table_row *)node->as.opaque)->is_header) {
        table_state->in_table_header = 1;
        cmark_strbuf_puts(html, "<thead>");
        cmark_html_render_cr(html);
      } else if (!table_state->need_closing_table_body) {
        cmark_strbuf_puts(html, "<tbody>");
        cmark_html_render_cr(html);
        table_state->need_closing_table_body = 1;
      }
      cmark_strbuf_puts(html, "<tr");
      cmark_html_render_sourcepos(node, html, options);
      cmark_strbuf_putc(html, '>');
    } else {
      cmark_html_render_cr(html);
      cmark_strbuf_puts(html, "</tr>");
      if (((node_table_row *)node->as.opaque)->is_header) {
        cmark_html_render_cr(html);
        cmark_strbuf_puts(html, "</thead>");
        table_state->in_table_header = false;
      }
    }
  } else if (node->type == CMARK_NODE_TABLE_CELL) {
    if (entering) {
      cmark_html_render_cr(html);
      if (table_state->in_table_header) {
        cmark_strbuf_puts(html, "<th");
      } else {
        cmark_strbuf_puts(html, "<td");
      }

      switch (get_cell_alignment(node)) {
      case 'l': html_table_add_align(html, "left", options); break;
      case 'c': html_table_add_align(html, "center", options); break;
      case 'r': html_table_add_align(html, "right", options); break;
      }

      cmark_html_render_sourcepos(node, html, options);
      cmark_strbuf_putc(html, '>');
    } else {
      if (table_state->in_table_header) {
        cmark_strbuf_puts(html, "</th>");
      } else {
        cmark_strbuf_puts(html, "</td>");
      }
    }
  } else {
    assert(false);
  }
}

static void opaque_alloc(cmark_syntax_extension *self, cmark_mem *mem, cmark_node *node) {
  if (node->type == CMARK_NODE_TABLE) {
    node->as.opaque = mem->calloc(1, sizeof(node_table));
  } else if (node->type == CMARK_NODE_TABLE_ROW) {
    node->as.opaque = mem->calloc(1, sizeof(node_table_row));
  } else if (node->type == CMARK_NODE_TABLE_CELL) {
    node->as.opaque = mem->calloc(1, sizeof(node_cell));
  }
}

static void opaque_free(cmark_syntax_extension *self, cmark_mem *mem, cmark_node *node) {
  if (node->type == CMARK_NODE_TABLE) {
    free_node_table(mem, node->as.opaque);
  } else if (node->type == CMARK_NODE_TABLE_ROW) {
    free_node_table_row(mem, node->as.opaque);
  }
}

static int escape(cmark_syntax_extension *self, cmark_node *node, int c) {
  return
    node->type != CMARK_NODE_TABLE &&
    node->type != CMARK_NODE_TABLE_ROW &&
    node->type != CMARK_NODE_TABLE_CELL &&
    c == '|';
}

cmark_syntax_extension *create_table_extension(void) {
  cmark_syntax_extension *self = cmark_syntax_extension_new("table");

  cmark_register_node_flag(&CMARK_NODE__TABLE_VISITED);
  cmark_syntax_extension_set_match_block_func(self, matches);
  cmark_syntax_extension_set_open_block_func(self, try_opening_table_block);
  cmark_syntax_extension_set_get_type_string_func(self, get_type_string);
  cmark_syntax_extension_set_can_contain_func(self, can_contain);
  cmark_syntax_extension_set_contains_inlines_func(self, contains_inlines);
  cmark_syntax_extension_set_commonmark_render_func(self, commonmark_render);
  cmark_syntax_extension_set_plaintext_render_func(self, commonmark_render);
  cmark_syntax_extension_set_latex_render_func(self, latex_render);
  cmark_syntax_extension_set_xml_attr_func(self, xml_attr);
  cmark_syntax_extension_set_man_render_func(self, man_render);
  cmark_syntax_extension_set_html_render_func(self, html_render);
  cmark_syntax_extension_set_opaque_alloc_func(self, opaque_alloc);
  cmark_syntax_extension_set_opaque_free_func(self, opaque_free);
  cmark_syntax_extension_set_commonmark_escape_func(self, escape);
  CMARK_NODE_TABLE = cmark_syntax_extension_add_node(0);
  CMARK_NODE_TABLE_ROW = cmark_syntax_extension_add_node(0);
  CMARK_NODE_TABLE_CELL = cmark_syntax_extension_add_node(0);

  return self;
}

uint16_t cmark_gfm_extensions_get_table_columns(cmark_node *node) {
  if (node->type != CMARK_NODE_TABLE)
    return 0;

  return ((node_table *)node->as.opaque)->n_columns;
}

uint8_t *cmark_gfm_extensions_get_table_alignments(cmark_node *node) {
  if (node->type != CMARK_NODE_TABLE)
    return 0;

  return ((node_table *)node->as.opaque)->alignments;
}

int cmark_gfm_extensions_set_table_columns(cmark_node *node, uint16_t n_columns) {
  return set_n_table_columns(node, n_columns);
}

int cmark_gfm_extensions_set_table_alignments(cmark_node *node, uint16_t ncols, uint8_t *alignments) {
  uint8_t *a = (uint8_t *)cmark_node_mem(node)->calloc(1, ncols);
  memcpy(a, alignments, ncols);
  return set_table_alignments(node, a);
}

int cmark_gfm_extensions_get_table_row_is_header(cmark_node *node)
{
  if (!node || node->type != CMARK_NODE_TABLE_ROW)
    return 0;

  return ((node_table_row *)node->as.opaque)->is_header;
}

int cmark_gfm_extensions_set_table_row_is_header(cmark_node *node, int is_header)
{
  if (!node || node->type != CMARK_NODE_TABLE_ROW)
    return 0;

  ((node_table_row *)node->as.opaque)->is_header = (is_header != 0);
  return 1;
}
