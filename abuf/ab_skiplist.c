#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include "ab_skiplist.h"

#define ABA_SLNODE(p)    ((ab_skipnode_t*)AB_ABA_P(ab_mem_item_t,p)->mem)

static ab_skipnode_t *ab_skiplist_new_node(ab_skiplist_t *list, ui8_t height, void *key, void *value);
static void ab_skiplist_free_node(ab_skiplist_t *list, ab_skipnode_t *node);
static ab_skipnode_t* ab_skiplist_find_ge(ab_skiplist_t *list, void *key, ui64_t incr, ab_skipnode_t** prev, ab_skipnode_t** forward);
static ab_skipnode_t* ab_skiplist_find_equal(ab_skiplist_t *list, void *key, ui64_t incr, ab_skipnode_t** prev, ab_skipnode_t** forward);
static ui8_t ab_skiplist_random_heigh(void);
static inline bool ab_skiplist_node_is_deleting(ab_skipnode_t* n);
static void ab_skiplist_incr_height(ab_skiplist_t *list, ui8_t height);
inline static void ab_skiplist_epoch_enter(ab_skiplist_t *list);
inline static void ab_skiplist_epoch_leave(ab_skiplist_t *list);

//interface
static bool ab_skiplist_insert_rf(ab_skiplist_t *list, void *key, ui32_t key_len, void *value, ui64_t *incr);
static void *ab_skiplist_delete_rf(ab_skiplist_t *list, void *key, ui32_t key_len, ui64_t incr);
static void *ab_skiplist_get_rf(ab_skiplist_t *list, void *key, ui32_t key_len, ui64_t incr);
static ab_skiplist_t *ab_skiplist_init_uniq(ui32_t attr, ab_compare_f compare, ab_print_f print, void *param);
static ab_skiplist_t *ab_skiplist_init_dupl(ui32_t attr, ab_compare_f compare, ab_print_f print, void *param);

ab_index_rf_t ab_index_rf_skiplist_uniq = 
{
    .init = (ab_init_f)ab_skiplist_init_uniq,
    .destory = (ab_destory_f)ab_skiplist_destory,
    .put = (ab_put_f)ab_skiplist_insert_rf,
    .del = (ab_del_f)ab_skiplist_delete_rf,
    .get = (ab_get_f)ab_skiplist_get_rf,
    .is_empty = (ab_is_empty_f)ab_skiplist_is_empty,
    .num = (ab_num_f)ab_skiplist_num,
};  

ab_index_rf_t ab_index_rf_skiplist_dupl = 
{
    .init = (ab_init_f)ab_skiplist_init_dupl,
    .destory = (ab_destory_f)ab_skiplist_destory,
    .put = (ab_put_f)ab_skiplist_insert_rf,
    .del = (ab_del_f)ab_skiplist_delete_rf,
    .get = (ab_get_f)ab_skiplist_get_rf,
    .is_empty = (ab_is_empty_f)ab_skiplist_is_empty,
    .num = (ab_num_f)ab_skiplist_num,
};  

static ab_skiplist_t *ab_skiplist_init_uniq(ui32_t attr, ab_compare_f compare, ab_print_f print, void *param)
{
    attr &= ~ab_attr_duplicate;
    return ab_skiplist_init(attr, compare, print, param);
}

static ab_skiplist_t *ab_skiplist_init_dupl(ui32_t attr, ab_compare_f compare, ab_print_f print, void *param)
{
    attr |= ab_attr_duplicate;
    return ab_skiplist_init(attr, compare, print, param);
}

ab_skiplist_t *ab_skiplist_init(ui32_t attr, ab_compare_f compare, ab_print_f print, void *param)
{
    int i = 0;
    int thread_safe = ab_sl_is_threadsafe(attr);
    ab_skiplist_t *list = (ab_skiplist_t *)ab_malloc(sizeof(ab_skiplist_t));
    if (list == NULL) return NULL;

    memset(list, 0, sizeof(ab_skiplist_t));

    //new all mem-pool
    list->mem1 = ab_mem_init(sizeof(ab_skipnode_t) + sizeof(ab_skipnode_t*)*3, thread_safe);
    list->mem2 = ab_mem_init(sizeof(ab_skipnode_t) + sizeof(ab_skipnode_t*)*AB_SKIPLIST_MAX_HEIGH, thread_safe);
    list->level_mem[0] = NULL;
    list->level_mem[1] = list->mem1;
    list->level_mem[2] = list->mem1;
    list->level_mem[3] = list->mem1;
    for (i = 4; i <= AB_SKIPLIST_MAX_HEIGH; i++) 
    {
        list->level_mem[i] = list->mem2;
    }
    
    //init other info
    list->attr = attr;
    list->incr_key = 1;
    list->height = 1;
    list->compare = compare;
    list->print = print;
    list->param = param;
    list->num = 0;

    //ab_skiplist_new_node has initralize next[] to NULL
    list->head = ab_skiplist_new_node(list, AB_SKIPLIST_MAX_HEIGH, NULL, NULL);
    if (!list->head)
    {
        ab_skiplist_destory(list);
        return NULL;
    }

    return list;
}

void ab_skiplist_destory(ab_skiplist_t *list)
{
    int i = 0;
    ab_skipnode_t* x = list->head;
    ab_skipnode_t* next = ABA_SLNODE(x)->next[0];
    
    while(next != NULL)
    {
        ab_skipnode_t* bak = ABA_SLNODE(next)->next[0];
        ab_skiplist_free_node(list, next);
        next = bak;
    }
    ab_skiplist_free_node(list, list->head);
    
    if (list->mem1) ab_mem_destory(list->mem1);
    if (list->mem2) ab_mem_destory(list->mem2);

    ab_free(list);
}

bool ab_skiplist_is_empty(ab_skiplist_t *list)
{
    return (ABA_SLNODE(list->head)->next[0] == NULL);
}

ui64_t ab_skiplist_num(ab_skiplist_t *list)
{
    return list->num;
}

bool ab_skiplist_insert_rf(ab_skiplist_t *list, void *key, ui32_t key_len, void *value, ui64_t *incr) 
{
    return (ab_skiplist_insert(list, key, value, incr) == ab_ret_ok);
}

ab_ret_t ab_skiplist_insert(ab_skiplist_t *list, void *key, void *value, ui64_t *incr) 
{
    int i = 0, cnt = 0;
    ui8_t height = ab_skiplist_random_heigh();
    ab_skipnode_t* prev[AB_SKIPLIST_MAX_HEIGH] = {0};
    ab_skipnode_t* forward[AB_SKIPLIST_MAX_HEIGH] = {0}; 
    ab_skipnode_t* x = NULL;        //with aba_mask address
    ab_skipnode_t* notex = NULL;    //real address
    ab_skipnode_t* tx = NULL;       //ge node

    //increase list height
    ab_skiplist_incr_height(list, height);

    //new node
    x = ab_skiplist_new_node(list, height, key, value);
    notex = ABA_SLNODE(x);
    notex->info.info.is_inserting = 1;

    ab_skiplist_epoch_enter(list);

    //search node and check uniq
    if((tx = ab_skiplist_find_ge(list, key, notex->info.info.incr_key, prev, forward)) != NULL)
    {
        if (!ab_sl_is_duplicate(list->attr) && list->compare(ABA_SLNODE(tx)->key, key, list->param) == 0)
        {
            ab_skiplist_free_node(list, x);
            ab_skiplist_epoch_leave(list);
            return ab_ret_skiplist_dupkey;
        }
    }
    
    for (i = 0; i < height; ) 
    {
        //check prev is deleting
        if (ab_skiplist_node_is_deleting(prev[i]))
        {
            ab_skiplist_find_ge(list, key, notex->info.info.incr_key, prev, forward);
            continue;  //retry set this level again
        }

        //x->n
        notex->next[i] = forward[i];

        //p->x->n
        //prev[i]->next[i] = x;
        if (!OS_CAS(&ABA_SLNODE(prev[i])->next[i], forward[i], x))
        {
            ab_skiplist_find_ge(list, key, notex->info.info.incr_key, prev, forward);
            continue;  //retry set this level again
        }

        i++;    //set next level
    }  

    notex->info.info.is_inserting = 0;
    if (incr) *incr = notex->info.info.incr_key;
    OS_ATOMIC_ADD(&list->num, 1);
    ab_skiplist_epoch_leave(list);
    
    return ab_ret_ok;
}

ab_ret_t ab_skiplist_delete_func(ab_skiplist_t *list, void *key, void **value, ui64_t *incr_io)
{
    int i;
    ab_skipnode_t* prev[AB_SKIPLIST_MAX_HEIGH] = {0};
    ab_skipnode_t* forward[AB_SKIPLIST_MAX_HEIGH] = {0};
    ab_skipnode_t* m = NULL;     //marker node
    ab_skipnode_t* x = NULL;     //delting node with with aba_mask address
    ab_skipnode_t* notex = NULL; //delteing node

    ab_skiplist_epoch_enter(list);

    if ((x = ab_skiplist_find_equal(list, key, *incr_io, prev, forward)) == NULL)
    {
        ab_skiplist_epoch_leave(list);
        return ab_ret_skiplist_norecord;
    }
    notex = ABA_SLNODE(x);
    
    //step1: set deleting flag
    ab_sln_info_t info_old = notex->info;
    ab_sln_info_t info_new = info_old;
    info_old.info.is_deleting  = 0;
    info_old.info.is_inserting = 0;
    info_new.info.is_deleting  = 1;
    if (!OS_CAS(&notex->info.ui64, info_old.ui64, info_new.ui64))
    {
        //other thread is deleting or inserting this node
        //return the updating node's incr_key, and caller function can retry delete next value by incr_io+1
        *incr_io = notex->info.info.incr_key;
        ab_skiplist_epoch_leave(list);
        return ab_ret_skiplist_updating; 
    }

    //new marker node
    m = ab_skiplist_new_node(list, notex->info.info.height, NULL, NULL);
    ABA_SLNODE(m)->info.info.is_marker = 1;

    for (i = notex->info.info.height-1; i >= 0; ) 
    {
        //The height of the deleted node may be greater than the height of the list when searching for nodes
        if (prev[i] == NULL) 
        {
            while(x != ab_skiplist_find_equal(list, key, notex->info.info.incr_key, prev, forward));
            continue;
        }

        //check prev node is deleting
        if (ab_skiplist_node_is_deleting(prev[i]))
        {
            while(x != ab_skiplist_find_equal(list, key, notex->info.info.incr_key, prev, forward));
            //assert(tx == x);
            continue;
        }

        //step2: set marker lock
        //TODO: 可以参考LF_HASH中，使用最低位设置位1表示删除标记，因为指针8字节对齐，低位不位1，这样节省一个marker节点
        ABA_SLNODE(m)->next[i] = forward[i];
        if ((notex->next[i] != m) && (!OS_CAS(&notex->next[i], forward[i], m)))
        {
            while(x != ab_skiplist_find_equal(list, key, notex->info.info.incr_key, prev, forward));
            //assert(tx == x);
            continue;
        }
        
        //step3: set link
        //prev[i]->next[i] = forward[i];
        if (!OS_CAS(&ABA_SLNODE(prev[i])->next[i], x, forward[i]))
        {
            while(x != ab_skiplist_find_equal(list, key, notex->info.info.incr_key, prev, forward));
            //assert(tx == x);
            continue;  //retry set this level again
        }

        i--;    //set next level link
    }

    //get value
    if (value) *value = notex->value;

    //free with epoch 
    ab_skiplist_free_node(list, x);
    ab_skiplist_free_node(list, m);

    /* TODO: 
    if (list->head->next[list->height-1] == NULL)
    {
        if (list->height > 1) list->height --;
    }
    */
    OS_ATOMIC_SUB(&list->num, 1);
    ab_skiplist_epoch_leave(list);

    return ab_ret_ok; 
}

void *ab_skiplist_delete_rf(ab_skiplist_t *list, void *key, ui32_t key_len, ui64_t incr)
{
    return ab_skiplist_delete(list, key, incr);
}

void *ab_skiplist_delete(ab_skiplist_t *list, void *key, ui64_t incr)
{
    void *value = NULL;
    ui64_t incr_io = incr;
    ab_ret_t ret =  ab_skiplist_delete_func(list, key, &value, &incr_io);
    
    // if support duplicate key, maybe the first value is deleting by other thread
    // then retry to delete next value which has the same key
    while (ret == ab_ret_skiplist_updating && 
           ab_sl_is_duplicate(list->attr)
           && incr == 0)
    {
        //incr_io += 1;   //retry to delete next value has the same key
                          //can not use incr_io+1, becasue maybe has incr_io+2/3.. nodes
        incr_io = incr;
        ret = ab_skiplist_delete_func(list, key, &value, &incr_io);
    }

    if (ret != ab_ret_ok) return NULL;
    return value;
}

void *ab_skiplist_get_rf(ab_skiplist_t *list, void *key, ui32_t key_len, ui64_t incr)
{
    return ab_skiplist_get(list, key, incr);
}

void *ab_skiplist_get(ab_skiplist_t *list, void *key, ui64_t incr)
{
    ab_skipnode_t* x = NULL;
    ab_skipnode_t* prev[AB_SKIPLIST_MAX_HEIGH] = {0};
    ab_skipnode_t* forward[AB_SKIPLIST_MAX_HEIGH] = {0}; 
    void *value = NULL;
    
    ab_skiplist_epoch_enter(list);

    x = ab_skiplist_find_ge(list, key, incr, prev, forward);
    if (x && list->compare(ABA_SLNODE(x)->key, key, list->param) == 0)
    {  
        value = ABA_SLNODE(x)->value;
        ab_skiplist_epoch_leave(list);
        return value;
    }

    ab_skiplist_epoch_leave(list);
    return NULL;
}

void ab_skiplist_print(ab_skiplist_t *list)
{
    int i, level;
    ab_skipnode_t* x = list->head;
    
    printf("Height: %u\n", list->height);
    for(level = 0; level < list->height; level++)
    {
        ab_skipnode_t* next = ABA_SLNODE(x)->next[level];
        printf("Level_%u: ", level);
        while(next != NULL)
        {
            if (ABA_SLNODE(next)->info.info.is_marker) printf("mark ");
            else if (list->print){
                list->print(ABA_SLNODE(next)->key, ABA_SLNODE(next)->value, list->param);
            }

            next = ABA_SLNODE(next)->next[level];
        }
        printf("\n");
    }
}

static ab_skipnode_t *ab_skiplist_new_node(ab_skiplist_t *list, ui8_t height, void *key, void *value)
{
    int i = 0;
    void *aba_node = ab_mem_alloc(list->level_mem[height]);
    ab_skipnode_t *node = ABA_SLNODE(aba_node); 
    if (node == NULL) return NULL;

    node->info.ui64 = 0;
    node->info.info.height = height;
    node->key = key;
    node->value = value;

    //IMPORTANT: when inserting a node, maybe heigher level has't linked to list
    for (i=0; i<height; i++) node->next[i] = NULL;

    //get incr key
    node->info.info.incr_key = 0;
    while (ab_sl_is_duplicate(list->attr))
    {
        ui64_t incr = list->incr_key;
        if (!OS_CAS(&list->incr_key, incr, incr+1)) continue;
        node->info.info.incr_key = incr;
        break;
    }

    return aba_node;
}

static void ab_skiplist_free_node(ab_skiplist_t *list, ab_skipnode_t *node)
{
    if (node)
    {
        int height = ABA_SLNODE(node)->info.info.height;
        ab_mem_free(list->level_mem[height], node);
    }
}

static ui8_t ab_skiplist_random_heigh(void) 
{
    int level = 1;
    while ((random()&0xFFFF) < (0.25 * 0xFFFF))
        level += 1;
    return (level<AB_SKIPLIST_MAX_HEIGH) ? level : AB_SKIPLIST_MAX_HEIGH;
}

static bool ab_skiplist_is_after_node(ab_skiplist_t *list, void *key, ui64_t incr, ab_skipnode_t* n) 
{
    int cmp = 0;
    ab_skipnode_t *node = ABA_SLNODE(n);

    if (n == NULL) return false;
    
    cmp = list->compare(node->key, key, list->param);
    if (cmp < 0) return true;
    else if (cmp > 0)  return false;
    else
    {
        return (node->info.info.incr_key < incr);
    }
}

static inline bool ab_skiplist_node_is_deleting(ab_skipnode_t* n)
{
    ab_skipnode_t *node = ABA_SLNODE(n);

    if (node->info.info.is_deleting)
    {
        return true;
    }

    if (node->info.info.is_marker)
    {
        return true;
    }

    return false;
}

static inline bool ab_skiplist_is_marker(ab_skipnode_t* n)
{
    return (n != NULL) && (ABA_SLNODE(n)->info.info.is_marker);
}


// find greater or equal node
// forward is greater then key or equal with key:
// 1) prev->forward(forward greater then key)
// 2) prev->forward(forward equal with key)
static ab_skipnode_t* ab_skiplist_find_ge(ab_skiplist_t *list, void *key, ui64_t incr, ab_skipnode_t** prev, ab_skipnode_t** forward)
{
    ab_skipnode_t *next = NULL;
    ab_skipnode_t *x = list->head;
    int level = list->height - 1;

    while (1) 
    {
        next = ABA_SLNODE(x)->next[level];
        if (ab_skiplist_is_marker(next))
        {
            //next is marker, skip this node
            next = ABA_SLNODE(next)->next[level];
        }
        if (ab_skiplist_is_after_node(list, key, incr, next)) 
        {
            x = next;
        } 
        else 
        {
            forward[level] = next;
            prev[level] = x;

            if (level == 0) 
            {
                return next;
            } 
            else 
            {
                level--;
            }
        }
    }
}

// find equal node
// prev->key->forward
static ab_skipnode_t *ab_skiplist_find_equal(ab_skiplist_t *list, void *key, ui64_t incr, ab_skipnode_t** prev, ab_skipnode_t** forward)
{
    int i = 0;
    ab_skipnode_t *x = NULL;
    ab_skipnode_t *notex = NULL;

    x = ab_skiplist_find_ge(list, key, incr, prev, forward);
    if (x == NULL)
    {
        return NULL;
    }

    notex = ABA_SLNODE(x);
    if (list->compare(notex->key, key, list->param) != 0)
    {
        return NULL;
    }
    if (incr != 0 && incr != notex->info.info.incr_key) //find by incr
    {
        return NULL;
    }

    //set to the x's forward node: prev->x->forward
    for (i = 0; i < notex->info.info.height; i++)
    {
        ab_skipnode_t * next = notex->next[i];
        if (ab_skiplist_is_marker(next)) forward[i] = ABA_SLNODE(next)->next[i];
        else forward[i] = next;
    }
    return x;
}

static void ab_skiplist_incr_height(ab_skiplist_t *list, ui8_t height)
{
    ui32_t new_height = height;

    for(;;)
    {
        ui32_t list_height = list->height;

        if (new_height <= list_height) return;
        if (!OS_CAS(&list->height, list_height, new_height))  continue;
    }
}

inline static void ab_skiplist_epoch_enter(ab_skiplist_t *list)
{
    ab_mem_epoch_enter(list->mem1);
    ab_mem_epoch_enter(list->mem2);
}  

inline static void ab_skiplist_epoch_leave(ab_skiplist_t *list)
{
    ab_mem_epoch_leave(list->mem2);
    ab_mem_epoch_leave(list->mem1);
}  
