#ifndef __AB_LIST__
#define __AB_LIST__

#include <stdbool.h>
#include "ab_mem.h"
#include "ab_index.h"

typedef struct ab_list_node
{
    struct ab_list_node *next;
    void *value;
}ab_list_node_t;

typedef struct ab_list
{
    ab_list_node_t *head;
    ab_list_node_t *tail;
    ab_mem_t *mem;
    ui64_t num;
}ab_list_t;

ab_list_t * ab_list_init();
void ab_list_destory(ab_list_t *list);

bool ab_list_put(ab_list_t *list, void *value);
void * ab_list_del(ab_list_t *list);
void * ab_list_get(ab_list_t *list);
ui64_t ab_list_num(ab_list_t *list);
bool ab_list_is_empty(ab_list_t *list);
void ab_list_foreach(ab_list_t *list, void (* callback)(void *value, void *param), void *param);

extern ab_index_rf_t ab_index_rf_list;

#endif
