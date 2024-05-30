#include <stdio.h>
#include <stdbool.h>
#include "ab_global.h"
#include "ab_list.h"

#define ABA_LNODE(p)    ((ab_list_node_t*)AB_ABA_P(ab_mem_item_t,p)->mem)

void* ab_list_init_f(ui32_t attr, ab_compare_f compare, ab_print_f print, void *param)
{
    return ab_list_init();
}
void ab_list_destory_f(void *containter)
{
    return ab_list_destory((ab_list_t *)containter);
}
bool ab_list_put_f(void *containter, void *key, ui8_t key_len, void *value, ui64_t *incr)
{
    return ab_list_put((ab_list_t *)containter, value);
}
void* ab_list_del_f(void *containter, void *key, ui8_t key_len, ui64_t incr)
{
    return ab_list_del((ab_list_t *)containter);
}
void* ab_list_get_f(void *containter, void *key, ui8_t key_len, ui64_t incr)
{
    return ab_list_get((ab_list_t *)containter);
}
bool ab_list_is_empty_f(void *containter)
{
    return ab_list_is_empty((ab_list_t *)containter);
}
ui64_t ab_list_num_f(void *containter)
{
    return ab_list_num((ab_list_t *)containter);
}

ab_index_rf_t ab_index_rf_list = 
{
    .init = (ab_init_f)ab_list_init_f,
    .destory = (ab_destory_f)ab_list_destory_f,
    .put = (ab_put_f)ab_list_put_f,
    .del = (ab_del_f)ab_list_del_f,
    .get = (ab_get_f)ab_list_get_f,
    .is_empty = (ab_is_empty_f)ab_list_is_empty_f,
    .num = (ab_num_f)ab_list_num_f,
};  

ab_list_t * ab_list_init()
{
    ab_list_t *list = NULL;
    ab_list_node_t *dummy = NULL;

    list = (ab_list_t *)ab_malloc(sizeof(ab_list_t));
    if (list == NULL)
    {
        return NULL;
    }

    list->num = 0;
    list->mem = ab_mem_init(sizeof(ab_list_node_t), 1);
    if (list->mem == NULL)
    {
        ab_free(list);
        return NULL;
    }

    dummy = (ab_list_node_t *)ab_mem_alloc(list->mem);
    if (dummy == NULL)
    {
        ab_mem_destory(list->mem);
        ab_free(list);
        return NULL;
    }

    ABA_LNODE(dummy)->next = NULL; 
    ABA_LNODE(dummy)->value = NULL;

    list->head = dummy; 
    list->tail = dummy; 
    
    return list;
}

void ab_list_destory(ab_list_t *list)
{
    if (list)
    {
        if (list->mem) ab_mem_destory(list->mem);
        ab_free(list);
    }
}

bool ab_list_put(ab_list_t *list, void *value)
{
    ab_list_node_t *tail = NULL;
    ab_list_node_t *next = NULL;
    ab_list_node_t *node = (ab_list_node_t *)ab_mem_alloc(list->mem);
    if (!node)
    {
        return false;
    }
    
    ABA_LNODE(node)->next = NULL;
    ABA_LNODE(node)->value = value;
    
    ab_mem_epoch_enter(list->mem);
    while(1) 
    {
        
        tail = list->tail;
        next = ABA_LNODE(tail)->next;
        
        if (tail != list->tail) continue;
        
        //tail has fail behind, reset tail
        if (next != NULL) 
        {
            OS_CAS(&list->tail, tail, next);
            continue;
        }
        
        //tail->next = node
        if (OS_CAS(&ABA_LNODE(tail)->next, next, node)) 
        {
            OS_ATOMIC_ADD(&list->num, 1);
            break;
        }
    }
 
    //set tail
    OS_CAS(&list->tail, tail, node);

    ab_mem_epoch_leave(list->mem);
    return true;
}

void * ab_list_del(ab_list_t *list)
{
    ab_list_node_t *head = NULL;
    ab_list_node_t *tail = NULL;
    ab_list_node_t *next = NULL;

    ab_mem_epoch_enter(list->mem);
    while(1) 
    {
        head = list->head;
        tail = list->tail;
        next = ABA_LNODE(head)->next;
 
        if (head != list->head)
        {
            continue;
        } 

        //empty list
        if (head == tail || next == NULL) 
        {
            ab_mem_epoch_leave(list->mem);
            return NULL;
        }

        //tail has fail behind, reset tail
        if (head == tail && next != NULL) 
        {
            OS_CAS(&list->tail, tail, next);
            continue;
        }

        //list->head = next
        void * value = ABA_LNODE(next)->value;
        if (OS_CAS(&list->head, head, next))
        {
            OS_ATOMIC_SUB(&list->num, 1);
            ab_mem_free(list->mem, head);
            ab_mem_epoch_leave(list->mem);
            return value;
        }
    }
}

void * ab_list_get(ab_list_t *list)
{
    ab_list_node_t *next = NULL;
    void *value = NULL;
    
    ab_mem_epoch_enter(list->mem);

    next = ABA_LNODE(list->head)->next;
    if (list->head == list->tail || next == NULL) 
    {
        return NULL;
    }
    
    value = ABA_LNODE(next)->value;
    
    ab_mem_epoch_leave(list->mem);
    return value;
}

bool ab_list_is_empty(ab_list_t *list)
{
    return (list->head == list->tail);
}

ui64_t ab_list_num(ab_list_t *list)
{
    return list->num;
}

//NOTICE: not support concurrrent !!
void ab_list_foreach(ab_list_t *list, void (* callback)(void *value, void *param), void *param)
{
    ab_list_node_t *next = ABA_LNODE(list->head)->next;

    while(next != NULL)
    {
        void *value = ABA_LNODE(next)->value;   //next maybe has modify or free by other thread
        callback(value, param);

        next = ABA_LNODE(next)->next;
    }
}

