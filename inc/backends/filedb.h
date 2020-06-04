#ifndef __H_VARSTOREDMEM_
#define __H_VARSTOREDMEM_

#include <stdint.h>
#include <stdbool.h>
#include <limits.h>

#include "common.h"

/**
 * This is a super simple backend for varstored.  It simply uses
 * a file-backed key-value store to maintain UEFI variables.
 *
 * Two DBs are used.  One maps the variable name to the variable value.
 * The other maps the variable name to the variable value len.
 * This is required because KISSDB only accepts fixed-length keys and values.
 */

#define min(x, y) ((x) < (y) ? (x) : (y))

#define ENTRY_LEN 1024
#define FILEDB_KEY_SIZE (MAX_VARNAME_SZ)
#define FILEDB_VAL_SIZE (MAX_VARDATA_SZ)
#define FILEDB_VAR_ATTRS_VAL_SIZE (sizeof(uint32_t))

int filedb_init(void);
void filedb_deinit(void);
int filedb_get(void *varname, size_t varname_len, void* dest,
               size_t dest_len, size_t *len, uint32_t *attrs);
int filedb_set(void *, size_t, void *, size_t, uint32_t);
void filedb_destroy(void);
int filedb_next(variable_t *current, variable_t *next);

#endif
