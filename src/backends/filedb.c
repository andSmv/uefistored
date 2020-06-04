#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>

#include "backends/backend.h"
#include "backends/filedb.h"
#include "common.h"
#include "kissdb/kissdb.h"

#define DEFAULT_DBPATH "/var/run/xen/varstored-db.dat"
#define DEFAULT_DBPATH_VAR_LEN "/var/run/xen/varstored-db-var-len.dat"
#define DEFAULT_DBPATH_VAR_ATTRS "/var/run/xen/varstored-db-var-attrs.dat"

static bool initialized;
static bool in_progress;

static KISSDB db;
static KISSDB db_var_len;
static KISSDB db_var_attrs;

#define CACHE_SIZE (MAX_VAR_COUNT)

static variable_t cache[CACHE_SIZE];
static int cache_len;

static char *_dbpath;
static char *_varlenpath;
static char *_attrspath;

int filedb_init(void)
{
    int ret;

    _dbpath = DEFAULT_DBPATH;
    _varlenpath = DEFAULT_DBPATH_VAR_LEN;
    _attrspath = DEFAULT_DBPATH_VAR_ATTRS;

    ret = KISSDB_open(&db, _dbpath, KISSDB_OPEN_MODE_RWCREAT,
                      CACHE_SIZE, FILEDB_KEY_SIZE, FILEDB_VAL_SIZE);
    if ( ret != 0 )
    {
        return -1;
    }

    ret = KISSDB_open(&db_var_len, _varlenpath, KISSDB_OPEN_MODE_RWCREAT,
                      1024, FILEDB_KEY_SIZE, sizeof(size_t));
    if ( ret != 0 )
        goto close1;

    ret = KISSDB_open(&db_var_attrs, _attrspath, KISSDB_OPEN_MODE_RWCREAT,
                      1024, FILEDB_KEY_SIZE, FILEDB_VAR_ATTRS_VAL_SIZE);
    if ( ret != 0 )
        goto close2;

    initialized = true;
    return 0;

close2:
    KISSDB_close(&db_var_len);

close1:
    KISSDB_close(&db);
    return -1;
}


void filedb_deinit(void)
{
    if ( !initialized )
        return;

    KISSDB_close(&db);
    KISSDB_close(&db_var_len);
    KISSDB_close(&db_var_attrs);
    initialized = false;
}

void filedb_destroy(void)
{
    if ( initialized )
        filedb_deinit();

    remove(_dbpath);
    remove(_varlenpath);
    remove(_attrspath);

    _dbpath = NULL;
    _varlenpath = NULL;
    _attrspath = NULL;

}

int filedb_get(void *varname, size_t varname_len, void* dest, size_t dest_len, size_t *len, uint32_t *attrs)
{
    int ret;
    uint8_t key[FILEDB_KEY_SIZE] = {0};
    uint8_t val[FILEDB_VAL_SIZE] = {0};
    size_t tmp = 0;

    if ( !initialized )
        return -1;

    if ( !varname )
        return -1;

    if ( !len )
        return -1;

    memcpy(&key, varname, varname_len);

    /* Get the variable's value */
    ret = KISSDB_get(&db, key, &val);
    if ( ret != 0 )
        return ret;

    /* Get the variable's value's length */
    ret = KISSDB_get(&db_var_len, key, &tmp);
    if ( ret != 0 )
        return ret;

    /* Get the variable's attrs */
    ret = KISSDB_get(&db_var_attrs, key, attrs);
    if ( ret != 0 )
        return ret;

    /* Copy only the correct length (from db_var_len) */
    if ( tmp == 0 )
    {
        ERROR("Failed to retrieve valid var length\n");
        return -1;
    }

    if ( dest_len < tmp )
    {
        ERROR("The dest_len (%lu) passed to %s was too small\n", dest_len, __func__);
        return -1;
    }

    memcpy(dest, val, tmp);
    *len = tmp;

    return 0;
}

int filedb_set(void *varname, size_t varlen, void *val, size_t len, uint32_t attrs)
{
    uint8_t key[FILEDB_KEY_SIZE] = {0};
    uint8_t padval[FILEDB_VAL_SIZE] = {0};
    int ret;

    if ( !initialized )
        return -1;

    if ( !varname )
        return -1;

    if ( !len )
        return -1;

    if ( varlen >=  FILEDB_KEY_SIZE )
    {
        ERROR("Variable name length exceeds db size\n");
        return -ENOMEM;
    }

    if ( len >=  FILEDB_VAL_SIZE )
    {
        ERROR("Variable data length exceeds db size\n");
        return -ENOMEM;
    }

    memcpy(&key, varname, varlen);
    memcpy(&padval, val, len);

    ret = KISSDB_put(&db, key, padval);
    if ( ret != 0 )
        return -1;

    ret = KISSDB_put(&db_var_len, key, &len);
    if ( ret != 0 )
        return -1;

    ret = KISSDB_put(&db_var_attrs, key, &attrs);
    if ( ret != 0 )
        return -1;

    return 0;
}

/**
 * Returns the cache entry that matches current.
 *
 * Returns a pointer to the next entry in the cache, or NULL if not found.
 *
 * @cache: The cache to search
 * @current: The variable to match against.
 */
static variable_t *find_next_cache_entry(variable_t cache[CACHE_SIZE], variable_t *current)
{
    int i;

    /* Empty variables are never found, return NULL for not found */
    if ( variable_is_empty(current) )
        return NULL;

    
    for ( i=0; i<cache_len; i++ )
    {
        if ( cache[i].namesz != current->namesz )
            continue;

        if (memcmp(&cache[i].name, current->name, current->namesz) == 0)
        {
            break;
        }
    }

    /*
     * If we've searched the whole cache and haven't found it,
     * return NULL for not found.
     *
     * If this is the last variable, then return NULL
     * (there is no next one).
     */
     
    
    if ( i >= cache_len - 1 )
        return NULL;

    
    return &cache[i + 1];
}

static void __populate_cache(void)
{
    variable_t *p;
    size_t tmp;
    static KISSDB_Iterator dbi;
    int ret;

    KISSDB_Iterator_init(&db, &dbi);
    
    cache_len = 0;

    /* Run the iterator to the end or until an error */
    p = cache;
    while ( KISSDB_Iterator_next(&dbi, &p->name, &p->data) > 0 )
    {
        p->namesz = strsize16((char16_t*)&p->name);

        /* Get the variable's value's length */
        ret = KISSDB_get(&db_var_len, &p->name, &tmp);
        if ( ret != 0 )
            return;

        p->datasz = tmp;
        cache_len++;
        p++;
    }
}

/**
 * Get the next variable in the DB
 *
 * @current: the current variable (provided by the caller)
 * @next: the next variable (loaded by this function)
 *
 * Returns -1 on error, 0 on end of list, 1 on success
 */
int filedb_next(variable_t *current, variable_t *next)
{
    variable_t *p;

    
    if ( !current || !next )
        return -1;
    

    if ( !in_progress )
    {
        if ( !variable_is_empty(current) )
            WARNING("OVMF is beginning a GetNextVariableName sequence with non-empty var\n");

        __populate_cache();
        in_progress = true;
    }
    else
    {
    
        /*
         * If a search is in progress and the user provides the empty string again,
         * we simply restart the iteration from the beginning.
         */
        if ( variable_is_empty(current) )
        {
    
            memset(cache, 0, sizeof(cache));
            __populate_cache();
            in_progress = true;
        }
    }

    
    /*
     * If current is the empty string, then the user is asking for the first
     * variable.  If there is no first variable, return not found.
     * Otherwise, return the first variable as next.
     */
    if ( variable_is_empty(current) )
    {
        if ( variable_is_empty(&cache[0]) )
            goto stop_iterator;

        memcpy(next, &cache[0], sizeof(*next));
        return 1;
    }

    p = find_next_cache_entry(cache, current);

    /* If not found, we've reached the end */
    if ( !p )
        goto stop_iterator;
    
    memcpy(next, p, sizeof(*p));
    return 1;

stop_iterator:
    in_progress = false;
    memset(cache, 0, sizeof(cache));
    cache_len = 0;
    return  0;
}

struct backend filedb_backend = {
    .init = filedb_init,
    .deinit = filedb_deinit,
    .get = filedb_get,
    .set = filedb_set,
    .destroy = filedb_destroy,
    .next = filedb_next,
};
