#include "chunk.h"
#include "cmark-gfm.h"

#ifdef __cplusplus
extern "C" {
#endif

bufsize_t _ext_scan_at(bufsize_t (*scanner)(const unsigned char *),
                       unsigned char *ptr, int len, bufsize_t offset);
bufsize_t _scan_table_start(const unsigned char *p);
bufsize_t _scan_table_cell(const unsigned char *p);
bufsize_t _scan_table_cell_end(const unsigned char *p);
bufsize_t _scan_table_row_end(const unsigned char *p);
bufsize_t _scan_tasklist(const unsigned char *p);

#define scan_table_start(c, l, n) _ext_scan_at(&_scan_table_start, c, l, n)
#define scan_table_cell(c, l, n) _ext_scan_at(&_scan_table_cell, c, l, n)
#define scan_table_cell_end(c, l, n) _ext_scan_at(&_scan_table_cell_end, c, l, n)
#define scan_table_row_end(c, l, n) _ext_scan_at(&_scan_table_row_end, c, l, n)
#define scan_tasklist(c, l, n) _ext_scan_at(&_scan_tasklist, c, l, n)

#ifdef __cplusplus
}
#endif
