#ifndef __AB_INDEX__
#define __AB_INDEX__

#include <stdbool.h>
#include "ab_global.h"

//index attr
#define ab_attr_duplicate        0x01
#define ab_attr_threadsafe       0x02

//index callback
typedef int (* ab_compare_f)(void *k1, void *k2, void *param);
typedef int (* ab_print_f)(void *k, void *v, void *param);

//index interface
typedef void* (*ab_init_f)(ui32_t attr, ab_compare_f compare, ab_print_f print, void *param);
typedef void  (*ab_destory_f)(void *containter);
typedef bool  (*ab_put_f)(void *containter, void *key, ui8_t key_len, void *value, ui64_t *incr);
typedef void* (*ab_del_f)(void *containter, void *key, ui8_t key_len, ui64_t incr);
typedef void* (*ab_get_f)(void *containter, void *key, ui8_t key_len, ui64_t incr);
typedef bool  (*ab_is_empty_f)(void *containter);
typedef ui64_t (*ab_num_f)(void *containter);

typedef struct ab_index_rf
{
    ab_init_f init; 
    ab_destory_f destory;
    ab_put_f put;
    ab_del_f del;
    ab_get_f get;
    ab_is_empty_f is_empty;
    ab_num_f num;
} ab_index_rf_t;

#endif
