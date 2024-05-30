#ifndef __AB_MEM__
#define __AB_MEM__

#include <stddef.h>
#include "ab_global.h"
#include "ab_os.h"

#define MAX_EPOCH_LIST 4

//#define AB_MEM_BLOCK_SIZE   (4096-sizeof(void *))

#define AB_ABA_PTR_MASK     0x0000FFFFFFFFFFFFULL
#define AB_ABA_CNT_MASK     0xFFFF000000000000ULL
#define AB_ABA_INCR         0x0001000000000000ULL

#define AB_ABA_P(t,p)       ((t *)((ui64_t)(p) & AB_ABA_PTR_MASK))

typedef struct ab_mem_item
{
    struct ab_mem_item *next;
    ui8_t mem[];
} ab_mem_item_t;

typedef struct ab_mem_block
{
    struct ab_mem_block *next;
    ui8_t block_buf[];
} ab_mem_block_t;

typedef struct ab_mem_epoch
{
    ui64_t thread_count;
    ui64_t global_epoch; 
    ui64_t local_epoches[MAX_THREAD_NUM];
    ui64_t active_flags[MAX_THREAD_NUM];
} ab_mem_epoch_t;

typedef struct ab_mem
{
    ui8_t  thread_safe;
    ui8_t  mem_rev;
    ui16_t mem_id;
    ui32_t slice_size; 
    ui32_t item_size; 
    ui32_t block_size;
    ui32_t num_per_block;
    
    ui64_t free_num;
    ab_mem_item_t *free_head;
    ab_mem_block_t *block_head;

    ui32_t logic_tick;
    ab_mem_epoch_t epoch;
    ab_mem_item_t *epoch_head[MAX_EPOCH_LIST];
} ab_mem_t;

ab_mem_t *ab_mem_init(ui32_t slice_size, ui8_t thread_safe);
void ab_mem_destory(ab_mem_t *mem);
void *ab_mem_alloc(ab_mem_t *mem);
void ab_mem_free(ab_mem_t *mem, void *p);
void ab_mem_epoch_enter(ab_mem_t *mem);
void ab_mem_epoch_leave(ab_mem_t *mem);


#define  MEM_POOL_ATOM_SIZE       32
#define  MEM_POOL_MAX_SIZE        100*1024
#define  MEM_POOL_ID_MAX          (ui16_t)((MEM_POOL_MAX_SIZE+MEM_POOL_ATOM_SIZE-1)/MEM_POOL_ATOM_SIZE)
#define  MEM_ID_FROM_SIZE(s)      (ui16_t)(((s) + MEM_POOL_ATOM_SIZE - 1)/MEM_POOL_ATOM_SIZE);
#define  MEM_ID_FROM_P(p)         (ui16_t)(((ui64_t)(p) >> 48) & 0xFFFF);

typedef struct 
{   
    ui32_t has_init;
    ab_mem_t* mem;
} ab_mpool_item_t;

typedef struct 
{
    ui32_t min_size;
    ui32_t max_size;
    ui16_t begin_id;
    ui16_t end_id;
    ui16_t mem_num;
    ui16_t pak;     //pack 8, make sure sizeof(ab_mpool_t) is the offset of pool[]
    ab_mpool_item_t pool[];
} ab_mpool_t;

ab_mpool_t *ab_mpool_init(ui32_t min_size, ui32_t max_size);
void ab_mpool_destory(ab_mpool_t *mpool);
void *ab_mpool_alloc(ab_mpool_t *mpool, ui32_t size);
void ab_mpool_free(ab_mpool_t *mpool, void *p);
void ab_mpool_epoch_enter(ab_mpool_t *mpool, ui16_t mem_id);
void ab_mpool_epoch_leave(ab_mpool_t *mpool);

void * ab_malloc(size_t s);
void ab_free(void *p);
ui64_t ab_mem_leak_check();

#endif
