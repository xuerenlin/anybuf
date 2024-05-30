#ifndef __AB_SKIPLIST__
#define __AB_SKIPLIST__

#include <stdbool.h>
#include "ab_error.h"
#include "ab_mem.h"
#include "ab_index.h"

#define AB_SKIPLIST_MAX_HEIGH  16

#define ab_sl_is_duplicate(attr)    ((attr)&ab_attr_duplicate)
#define ab_sl_is_threadsafe(attr)   ((attr)&ab_attr_threadsafe)

typedef union ab_sln_info_t
{
    struct 
    {
        ui64_t is_marker    : 1;
        ui64_t is_deleting  : 1;
        ui64_t is_inserting : 1;
        ui64_t height       : 5;
        ui64_t incr_key     : 56;
    } info;
    ui64_t ui64;
}ab_sln_info_t;

typedef struct ab_skipnode_t
{
    void *key;
    void *value;
    ab_sln_info_t info;
    struct ab_skipnode_t *next[0];
} ab_skipnode_t;

typedef struct ab_skiplist_t
{ 
    ui32_t attr;
    ui32_t height;      //use ui32_t type but not ui8_t for CAS
    ui64_t incr_key;
    ui64_t num;
    ab_skipnode_t *head;
    ab_compare_f compare;
    ab_print_f print;
    void * param;

    ab_mem_t *mem1;
    ab_mem_t *mem2;
    ab_mem_t *level_mem[AB_SKIPLIST_MAX_HEIGH+1];
} ab_skiplist_t;

ab_skiplist_t *ab_skiplist_init(ui32_t attr, ab_compare_f compare, ab_print_f print, void *param);
void ab_skiplist_destory(ab_skiplist_t *list);

ab_ret_t ab_skiplist_insert(ab_skiplist_t *list, void *key, void *value, ui64_t *incr);
void *ab_skiplist_delete(ab_skiplist_t *list, void *key, ui64_t incr);
void *ab_skiplist_get(ab_skiplist_t *list, void *key, ui64_t incr);
bool ab_skiplist_is_empty(ab_skiplist_t *list);
ui64_t ab_skiplist_num(ab_skiplist_t *list);
void ab_skiplist_print(ab_skiplist_t *list);

ab_index_rf_t ab_index_rf_skiplist_uniq;
ab_index_rf_t ab_index_rf_skiplist_dupl;


#endif
