#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sys/time.h>
#include "ab_mem.h"

static ui64_t g_tick = 0;

void * ab_malloc(size_t s)
{
    OS_ATOMIC_ADD(&g_tick, 1);
    return malloc(s);
}

void ab_free(void *p)
{
    OS_ATOMIC_SUB(&g_tick, 1);
    free(p);
}

ui64_t ab_mem_leak_check()
{
    return g_tick;
}

#define ABA_ITEM(p)          AB_ABA_P(ab_mem_item_t, p)
#define ABA_MASK_INCR(f,t)   ((((ui64_t)(f)+AB_ABA_INCR)&AB_ABA_CNT_MASK) | ((ui64_t)(t)&AB_ABA_PTR_MASK))


static void ab_mem_epoch_init(ab_mem_epoch_t *epoch);
static void ab_mem_epoch_gc(ab_mem_t *mem, ui64_t epoch_id);
static void ab_mem_epoch_increase(ab_mem_t *mem);
static ab_mem_item_t *ab_mem_new_block(ab_mem_t *mem);
static ab_mem_item_t *ab_mem_get_head(ab_mem_t *mem, ab_mem_item_t **which_head);
static void ab_mem_put_head(ab_mem_t *mem, ab_mem_item_t **which_head, void *p);

static inline ui32_t ab_mem_num_per_block(ui32_t block_size, ui32_t item_size)
{
    return (block_size - (ui32_t)(size_t)&((ab_mem_block_t*)0)->block_buf)/item_size;
}

ab_mem_t *ab_mem_init_with_id(ui16_t mem_id, ui32_t slice_size, ui8_t thread_safe)
{
    int i = 0;
    ab_mem_t *mem = NULL;
    ui32_t pagesize = ab_get_pagesize();

    mem = (ab_mem_t *)ab_malloc(sizeof(ab_mem_t));
    if (!mem) return NULL;

    mem->mem_id = mem_id;
    mem->thread_safe = thread_safe;
    mem->slice_size = slice_size;
    mem->item_size = sizeof(ab_mem_item_t) + slice_size;

    mem->num_per_block = 0;
    mem->block_size = pagesize;
    while(mem->num_per_block < 5)
    {
        mem->block_size = mem->block_size*2;
        mem->num_per_block = ab_mem_num_per_block(mem->block_size, mem->item_size);
    }
    
    mem->free_head = NULL;
    mem->block_head = NULL;
    mem->free_num = 0;

    if (!ab_mem_new_block(mem))
    {
        ab_free(mem); 
        return NULL;
    }

    ab_mem_epoch_init(&mem->epoch);
    for (i=0; i<MAX_EPOCH_LIST; i++)
        mem->epoch_head[i] = NULL;

    return mem;
}

ab_mem_t *ab_mem_init(ui32_t slice_size, ui8_t thread_safe)
{
    return ab_mem_init_with_id(0, slice_size, thread_safe);
}

void ab_mem_destory(ab_mem_t *mem)
{
    int freenum = 0;
    ab_mem_block_t *block = NULL;
    
    if (!mem) return;

    block = mem->block_head;
    while(block)
    {   
        ab_mem_block_t *cur = block;
        block = block->next;

        freenum++;
        ab_free(cur);
    }
    //printf("has free %d blocks, Epoch: %lu Threads: %lu\n", freenum, mem->epoch.global_epoch, mem->epoch.thread_count);

    ab_free(mem);
}

void *ab_mem_alloc(ab_mem_t *mem)
{
    ab_mem_item_t *item = NULL;

    for(;;)
    {
        item = ab_mem_get_head(mem, &mem->free_head);
        if (item == NULL) 
        {
            if (!ab_mem_new_block(mem))
            {
                return NULL;
            }
            continue;
        }
        
        //set mem-id
        if (mem->mem_id)
        {
            //item = (ab_mem_item_t *)(((ui64_t)item & AB_ABA_PTR_MASK) | ((ui64_t)mem->mem_id<<48)); 
            ABA_ITEM(item)->next = (ab_mem_item_t *)(size_t)mem->mem_id;
        }
        
        return item;
    }
}

void ab_mem_free(ab_mem_t *mem, void *p)
{
    if (!mem->thread_safe) //single threaded implementation
    {
        ab_mem_put_head(mem, &mem->free_head, p);
    }
    else //thread safe implementation
    {
        //logic free, put to current epoch_list
        ui64_t cur_epoch = mem->epoch.global_epoch;
        ab_mem_put_head(mem, &mem->epoch_head[cur_epoch%MAX_EPOCH_LIST], p);

        //check increase epoch
        //check increase global epoch per 256 logic-free
        //delay real free item, reduce the probability of ABA problems occurring 
        OS_ATOMIC_ADD(&mem->logic_tick, 1);
        if ((mem->logic_tick & 0x000000FF) == 0x000000FF)
        {
            ab_mem_epoch_increase(mem);
        }
    }
}

//get item from head
static ab_mem_item_t *ab_mem_get_head(ab_mem_t *mem, ab_mem_item_t **which_head)
{
    ab_mem_item_t *free_head = NULL;
    ab_mem_item_t *free_head_ptr = NULL;
    ab_mem_item_t *head_next = NULL;

    //single threaded implementation
    if (!mem->thread_safe)
    {
        free_head = *which_head;
        if (free_head == NULL)
        {
            return NULL;
        }
        *which_head = ABA_ITEM(free_head)->next;
        return (void *)free_head;
    }

    //thread safe implementation
    for(;;)
    {
        free_head = *which_head;
        free_head_ptr = ABA_ITEM(free_head);
        if (free_head_ptr == NULL)
        {
            return NULL;
        }

        head_next = free_head_ptr->next;
        
        //set new aba counter
        head_next = (ab_mem_item_t *)ABA_MASK_INCR(free_head, head_next);

        if (OS_CAS(which_head, free_head, head_next)) 
        {  
            return (void *)free_head;
        }
    }
}

//put item to head
static void ab_mem_put_head(ab_mem_t *mem, ab_mem_item_t **which_head, void *p)
{
    ab_mem_item_t *item = (ab_mem_item_t *)p;
    ab_mem_item_t *free_head = NULL;

    if (p == NULL || ABA_ITEM(p) == NULL)
    {
        return;
    }

    //single threaded implementation
    if (!mem->thread_safe)
    {
        free_head = *which_head;
        ABA_ITEM(item)->next = free_head;
        *which_head = item;
        return;
    }

    //thread safe implementation
    for(;;)
    {
        free_head = *which_head;

        //item->next = free_head
        ABA_ITEM(item)->next = free_head;

        //set new aba counter
        item = (ab_mem_item_t *)ABA_MASK_INCR(free_head, item);

        //mem->free_head = item;
        if (OS_CAS(which_head, free_head, item))
        {
            break;
        } 
    }
}

//return last item in new block
static ab_mem_item_t *ab_mem_new_block(ab_mem_t *mem)
{
    int i = 0;
    ab_mem_item_t *first = NULL;
    ab_mem_item_t *last = NULL;
    ab_mem_item_t *free_head = NULL;
    ab_mem_block_t *block_head = NULL;
    ab_mem_block_t *block = NULL;

    block = (ab_mem_block_t *)ab_malloc(mem->block_size);
    if (!block) 
    {
        return NULL;
    }

    //create list in block
    first = (ab_mem_item_t *)block->block_buf;
    for (i=0; i<mem->num_per_block; i++)
    {
        last = (ab_mem_item_t *)&block->block_buf[i*mem->item_size];
        last->next = (ab_mem_item_t *)&block->block_buf[(i+1)*mem->item_size];
    }
    last->next = NULL;  //last one
    
    //single threaded implementation
    if (!mem->thread_safe)
    {
        //link to free_head
        last->next = free_head;
        mem->free_head = first;

        //link to block_head
        block->next = mem->block_head;
        mem->block_head = block;
        return last;
    }

    //thread safe implementation
    //link to free_head
    do {
        free_head = mem->free_head;
        last->next = free_head;
    }while(!OS_CAS(&mem->free_head, free_head, first)); //mem->free_head = first;
    OS_ATOMIC_ADD(&mem->free_num, mem->num_per_block);

    //link to block_head
    do {
        block_head = mem->block_head;
        block->next = block_head;
    }while(!OS_CAS(&mem->block_head, block_head, block)); //mem->block_head = block;

    return last;
}


//////////////////////////////////////////////////////////////////////////////////////////
// epoch implementation
//////////////////////////////////////////////////////////////////////////////////////////

static int ab_mem_epoch_thread_id(ab_mem_epoch_t *epoch)
{
    int id = ab_get_thread_id();

    if (epoch->thread_count < id+1) epoch->thread_count = id+1;
    
    return id;
}

static void ab_mem_epoch_init(ab_mem_epoch_t *epoch)
{
    int i = 0;

    epoch->global_epoch = 0;
    epoch->thread_count = 0;
    
    for (i = 0; i < MAX_THREAD_NUM; ++i) 
    {
        epoch->local_epoches[i] = 0;
        epoch->active_flags[i] = 0;
    }
}

void ab_mem_epoch_enter(ab_mem_t *mem)
{
    if (mem->thread_safe)
    {
        int tid = ab_mem_epoch_thread_id(&mem->epoch);
        mem->epoch.active_flags[tid] = 1;
        CPU_BARRIER(); //cpu barrier after write flags and before read global_epoch
        mem->epoch.local_epoches[tid] = mem->epoch.global_epoch;
        //CPU_BARRIER(); 
        //NO need call cpu_barrier after set local_epoches 
        //If other thread can not read the value immediately, just cause increase epoch delay, it's not matter
    } 
}

void ab_mem_epoch_leave(ab_mem_t *mem)
{
    if (mem->thread_safe) 
    {
        mem->epoch.active_flags[ab_mem_epoch_thread_id(&mem->epoch)] = 0;
        //CPU_BARRIER(); 
        //NO need call cpu_barrier after set active_flags = 0, it where called before read
    }
}

static void ab_mem_epoch_gc(ab_mem_t *mem, ui64_t epoch_id)
{
    ab_mem_item_t *item = NULL;
    
    while((item = ab_mem_get_head(mem, &mem->epoch_head[epoch_id%MAX_EPOCH_LIST])) != NULL)
    {
        ab_mem_put_head(mem, &mem->free_head, item);
    }
}

static void ab_mem_epoch_increase(ab_mem_t *mem) 
{
    int i = 0;
    ab_mem_epoch_t *epoch = &mem->epoch;
    ui64_t global_epoch = 0;
    ui64_t next_epoch = 0;
    
    CPU_BARRIER();  //cpu barrier before read
    global_epoch = epoch->global_epoch;
    next_epoch = global_epoch + 1;

    for (i = 0; i < epoch->thread_count; i++) 
    {
        CPU_BARRIER();  //cpu barrier before read
        if (epoch->active_flags[i] == 1 && epoch->local_epoches[i] != global_epoch) 
        {
            return;
        }
    }
    
    //use cas increase global epoch, make sure that global_epoch +1 at one time, and only one thread try gc
    if(OS_CAS(&epoch->global_epoch, global_epoch, next_epoch))
    {
        ab_mem_epoch_gc(mem, epoch->global_epoch+1);
    }
}


///////////////////////////////////////////////////////////////////////////////////////////
// mem-pool implementation
//
// Only used for ab_buf's value, because do not with ABA-MASK but with MEM-ID(begin with 1)
///////////////////////////////////////////////////////////////////////////////////////////

#define MEM_ID_HAS_INIT(mpool,mem_id)  ((mpool)->pool[(mem_id) - (mpool)->begin_id].has_init == 2)

void ab_mpool_destory(ab_mpool_t *mpool)
{
    ui16_t i = 0;
    for (i=0; i<mpool->mem_num; i++)
    {
        if (mpool->pool[i].mem) ab_mem_destory(mpool->pool[i].mem);
        mpool->pool[i].mem = NULL;
        mpool->pool[i].has_init = 0;
    }
    ab_free(mpool);
}

ab_mpool_t *ab_mpool_init(ui32_t min_size, ui32_t max_size)
{
    ab_mpool_t *mpool = NULL;
    ui16_t begin_id = MEM_ID_FROM_SIZE(min_size);
    ui16_t end_id = MEM_ID_FROM_SIZE(max_size);
    size_t sz = 0;
    
    if (end_id < begin_id) return NULL;
    
    sz = sizeof(ab_mpool_t) + (end_id-begin_id+1)*sizeof(ab_mpool_item_t);
    mpool = (ab_mpool_t *)ab_malloc(sz);
    if (!mpool) return NULL;

    memset(mpool, 0, sz);
    mpool->min_size = min_size;
    mpool->max_size = max_size;
    mpool->begin_id = begin_id;
    mpool->end_id   = end_id;
    mpool->mem_num  = end_id - begin_id + 1;

    return mpool;
}

bool ab_mpool_init_one(ab_mpool_t *mpool, ui16_t mem_id)
{
    ui16_t id = mem_id - mpool->begin_id;
    if (mem_id < mpool->begin_id || mem_id > mpool->end_id) return false;

    while(mpool->pool[id].has_init != 2)
    {
        if(!OS_CAS(&mpool->pool[id].has_init, 0, 1)) continue;
        
        //printf("first ab_mpool_init_one %u\n", mem_id);
        mpool->pool[id].mem = ab_mem_init_with_id(mem_id, (ui32_t)mem_id*MEM_POOL_ATOM_SIZE, 1);
        if (!mpool->pool[id].mem)
        {
            mpool->pool[id].has_init = 0;
            return false;
        }
        mpool->pool[id].has_init = 2;
    }
    return true;
}

void *ab_mpool_alloc(ab_mpool_t *mpool, ui32_t size)
{
    ui16_t mem_id = MEM_ID_FROM_SIZE(size);

    if (mem_id > MEM_POOL_ID_MAX) return NULL;
    if (!MEM_ID_HAS_INIT(mpool,mem_id) && !ab_mpool_init_one(mpool,mem_id)) return NULL;
    return ab_mem_alloc(mpool->pool[mem_id - mpool->begin_id].mem);
}

void ab_mpool_free(ab_mpool_t *mpool, void *p)
{
    //ui16_t mem_id = (ui16_t)(((ui64_t)p >> 48) & 0xFFFF);
    ui16_t mem_id = (ui16_t)(size_t)ABA_ITEM(p)->next;

    if (mem_id > MEM_POOL_ID_MAX) return;
    if (!MEM_ID_HAS_INIT(mpool, mem_id) && !ab_mpool_init_one(mpool, mem_id)) return;
    ab_mem_free(mpool->pool[mem_id - mpool->begin_id].mem, p);
}

void ab_mpool_epoch_enter(ab_mpool_t *mpool, ui16_t mem_id)
{
    ui16_t i = 0;
    
    if (mem_id != 0)
    {
        if (!MEM_ID_HAS_INIT(mpool, mem_id) && !ab_mpool_init_one(mpool, mem_id)) return;
    }

    for (i = mpool->begin_id; i<=mpool->end_id; i++)
    {
        if (i == mem_id || MEM_ID_HAS_INIT(mpool, i))
        {
            ab_mem_epoch_enter(mpool->pool[i - mpool->begin_id].mem);
        }
    }
}

void ab_mpool_epoch_leave(ab_mpool_t *mpool)
{
    ui16_t i = 0;

    for (i = mpool->begin_id; i<=mpool->end_id; i++)
    {
        if (MEM_ID_HAS_INIT(mpool, i))
        {
            ab_mem_epoch_leave(mpool->pool[i - mpool->begin_id].mem);
        }
    }
}
