#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "ab_hash.h"


#define PTR_D(p) ((ui64_t)p|1)  //set delete flag
#define PTR_P(p) ((ui64_t)p&0xFFFFFFFFFFFFFFFEULL) //delete delete flag
#define ABA_HNODE(p)    ((ab_hash_node_t*) (AB_ABA_P(ab_mem_item_t,PTR_P(p))->mem)) 

static inline ui32_t ab_reverse_bits(ui32_t key);
static inline ui32_t ab_clear_highest_bit(ui32_t v);
static ui32_t hashfunc(ui32_t seed, const void *key, int len);
static ab_hash_node_t *ab_hash_new_dummy(ab_hash_t *hash, ui32_t bucket);
static ab_hash_node_t *ab_hash_new_node(ab_hash_t *hash, ui32_t h, void *key, ui8_t key_len, void *value);
static ab_hash_node_t *ab_hash_entry_insert(ab_hash_t *hash, ui32_t bucket);
static ab_hash_node_t *ab_hash_entry_get(ab_hash_t *hash, ui32_t bucket);
static bool ab_hash_list_insert(ab_hash_t *hash, ab_hash_node_t *entry, ab_hash_node_t *node, ui32_t hashnr);
static bool ab_hash_list_delete(ab_hash_t *hash, ab_hash_node_t *entry, ab_hash_node_t *prev, ab_hash_node_t *node, ui32_t hashnr);
static void ab_hash_check_resize(ab_hash_t *hash);
static ab_hash_node_t *ab_hash_find_key(ab_hash_t *hash, ab_hash_node_t *entry, ui32_t hashnr, void *key, ui8_t key_len, ab_hash_node_t **prev);
static ab_hash_node_t *ab_hash_find_node(ab_hash_t *hash, ab_hash_node_t *prev, ui32_t hashnr, ab_hash_node_t *target);


ab_index_rf_t ab_index_rf_hash = 
{
    .init = (ab_init_f)ab_hash_init,
    .destory = (ab_destory_f)ab_hash_destory,
    .put = (ab_put_f)ab_hash_insert,
    .del = (ab_del_f)ab_hash_delete,
    .get = (ab_get_f)ab_hash_get,
    .is_empty = (ab_is_empty_f)ab_hash_is_empty,
    .num = (ab_num_f)ab_hash_num,
};  

ab_hash_t *ab_hash_init(ui32_t attr, ab_compare_f compare, ab_print_f print, void *param)
{
    ab_hash_t *hash = NULL;
    ui32_t thread_safe = (attr&ab_attr_threadsafe);

    hash = (ab_hash_t *)ab_malloc(sizeof(ab_hash_t));
    if (hash == NULL)
    {
        return NULL;
    }
    memset(hash, 0, sizeof(ab_hash_t));
    hash->num = 0;
    hash->size = 2;
    hash->maxload = hash->size/2;
    hash->mem = ab_mem_init(sizeof(ab_hash_node_t), thread_safe);
    if (hash->mem == NULL)
    {
        ab_free(hash);
        return NULL;
    }
    hash->list_head = ab_hash_new_dummy(hash, 0);
    if (!hash->list_head)
    {
        ab_hash_destory(hash);
    }
    hash->compare = compare;
    hash->param = param;

    return hash;
}

void ab_hash_destory(ab_hash_t *hash)
{
    ui32_t i = 0;

    if (!hash) return;
    
    for(i=0; i<ENTRYS_PER_BLOCK; i++) 
    {
        if (hash->entry_blocks[i]) 
        {
            ab_free(hash->entry_blocks[i]);
        }
    }

    if (hash->mem) ab_mem_destory(hash->mem);

    ab_free(hash);
}

bool ab_hash_insert(ab_hash_t *hash, void *key, ui8_t keylen, void *value, ui64_t *incr)
{
    ui32_t h = hashfunc(0, key, keylen);
    ui32_t bucket = h % hash->size;
    ab_hash_node_t *entry = NULL;
    ab_hash_node_t *n = NULL;

    ab_mem_epoch_enter(hash->mem);

    n = ab_hash_new_node(hash, h, key, keylen, value);
    if (!n) return false;

    if((entry = ab_hash_entry_insert(hash, bucket)) != NULL)
    {
        if (ab_hash_list_insert(hash, entry, n, ABA_HNODE(n)->hashnr))
        {
            OS_ATOMIC_ADD(&hash->num, 1);
            ab_hash_check_resize(hash);
            ab_mem_epoch_leave(hash->mem);
            return true;
        }
    }

    ab_mem_free(hash->mem, n);
    ab_mem_epoch_leave(hash->mem);
    return false;
}

void *ab_hash_get(ab_hash_t *hash, void *key, ui8_t keylen, ui64_t incr)
{
    ui32_t h = hashfunc(0, key, keylen);
    ui32_t hashnr = ab_reverse_bits(h) | 1;
    ui32_t bucket = h % hash->size;
    ab_hash_node_t *n = NULL, *prev = NULL;
    ab_hash_node_t *entry = NULL;

    ab_mem_epoch_enter(hash->mem);

    if ((entry = ab_hash_entry_get(hash, bucket)) != NULL)
    {
        if((n = ab_hash_find_key(hash, entry, hashnr, key, keylen, &prev)) != NULL)
        {
            void *value = ABA_HNODE(n)->value;
            ab_mem_epoch_leave(hash->mem);

            return value;
        }
    }

    ab_mem_epoch_leave(hash->mem);
    return NULL;
}

void *ab_hash_delete(ab_hash_t *hash, void *key, ui8_t keylen, ui64_t incr)
{
    ui32_t h = hashfunc(0, key, keylen);
    ui32_t hashnr = ab_reverse_bits(h) | 1;
    ui32_t bucket = h % hash->size;
    ab_hash_node_t *n = NULL, *prev = NULL;
    ab_hash_node_t *entry = NULL;
    
    ab_mem_epoch_enter(hash->mem);

    if ((entry = ab_hash_entry_get(hash, bucket)) != NULL)
    {
        if ((n = ab_hash_find_key(hash, entry, hashnr, key, keylen, &prev)) != NULL)
        {
            if (ab_hash_list_delete(hash, entry, prev, n, hashnr))
            {
                void *value = ABA_HNODE(n)->value;
                OS_ATOMIC_SUB(&hash->num, 1);
                ab_mem_epoch_leave(hash->mem);

                return value;
            }
        }
    }

    ab_mem_epoch_leave(hash->mem);
    return NULL;
}

ui64_t ab_hash_num(ab_hash_t *hash)
{
    return hash->num;
}

bool ab_hash_is_empty(ab_hash_t *hash)
{
    return (hash->num == 0);
}

//NOTICE: just for test
void ab_hash_foreach(ab_hash_t *hash, void (* callback)(void *key, ui32_t hashnr, void *param), void *param)
{
    ab_hash_node_t *next = hash->list_head;

    while(next != NULL)
    {
        ab_hash_node_t *real = ABA_HNODE(next);
        if (!real->is_dummy)
            callback(real->key, real->hashnr, param);

        next = real->next;
    }
}

static void ab_hash_check_resize(ab_hash_t *hash)
{
    ui64_t size = hash->size;
    ui64_t maxload = hash->maxload;

    if (hash->num >= maxload && hash->size < BUCKET_MAX)
    {
        OS_CAS(&hash->size, size, size*2);
    }

    size = hash->size;
    maxload = hash->maxload;
    if (maxload*2 != size)
    {
        OS_CAS(&hash->maxload, maxload, size/2);
    }
}

static inline int ab_hash_cmp(ab_hash_t *hash, ab_hash_node_t *node, void *key, ui8_t key_len)
{
    if (node->is_dummy) return 1;
    if (hash->compare) return hash->compare(node->key, key, hash->param);
    if ((node->key_len == key_len) && memcmp(node->key, key, key_len) == 0) return 0;
    return 1;
}

static ab_hash_node_t *ab_hash_find_key(ab_hash_t *hash, ab_hash_node_t *entry, ui32_t hashnr, void *key, ui8_t key_len, ab_hash_node_t **prev)
{
    ab_hash_node_t *next_node = NULL;
    ab_hash_node_t *next = ABA_HNODE(entry)->next;
    
    *prev = entry;
    while(PTR_P(next))
    {
        next_node = ABA_HNODE(next);

        if (next_node->hashnr > hashnr) break;
        if (ab_hash_cmp(hash, next_node, key, key_len) == 0) return next;

        *prev = next;
        next = next_node->next;
    }

    return NULL;
}

static ab_hash_node_t *ab_hash_find_ge(ab_hash_t *hash, ab_hash_node_t *prev, ui32_t hashnr, void *key, ui8_t key_len, ab_hash_node_t **next_out)
{
    ab_hash_node_t *prev_node = NULL;
    ab_hash_node_t *next_node = NULL;
    ab_hash_node_t *next = NULL;

    while(PTR_P(prev))
    {
        prev_node = ABA_HNODE(prev);
        next = prev_node->next;
        next_node = ABA_HNODE(next);

        if (!PTR_P(next) || next_node->hashnr > hashnr) break;
        if (key && ab_hash_cmp(hash, next_node, key, key_len) == 0) return NULL;   //dup-key

        prev = next;
    }

    *next_out = next;
    return prev;
}

static ab_hash_node_t *ab_hash_find_node(ab_hash_t *hash, ab_hash_node_t *prev, ui32_t hashnr, ab_hash_node_t *target)
{
    ab_hash_node_t *prev_node = NULL;
    ab_hash_node_t *next_node = NULL;
    ab_hash_node_t *next = NULL;

    while(PTR_P(prev))
    {
        prev_node = ABA_HNODE(prev);
        next = prev_node->next;
        next_node = ABA_HNODE(next);

        if (PTR_P(next) == PTR_P(target)) break;  //ok 
        if (!PTR_P(next) || next_node->hashnr >= hashnr) return NULL;
        
        prev = next;
    }
    return prev;
}

static bool ab_hash_list_insert(ab_hash_t *hash, ab_hash_node_t *entry, ab_hash_node_t *new, ui32_t hashnr)
{
    ab_hash_node_t *next = NULL;
    ab_hash_node_t *prev = entry;
    ab_hash_node_t *prev_node = NULL;
    ab_hash_node_t *new_node = ABA_HNODE(new);

    for(;;)
    {
        prev = ab_hash_find_ge(hash, entry, hashnr, new_node->key, new_node->key_len, &next);
        if (!prev) return false;

        //new->next = next
        ABA_HNODE(new)->next = (ab_hash_node_t *)PTR_P(next);

        //prev->next = new
        prev_node = ABA_HNODE(prev);
        if (OS_CAS(&prev_node->next, PTR_P(next), new))  break;
    }

    return true;
}

static bool ab_hash_list_delete(ab_hash_t *hash, ab_hash_node_t *entry, ab_hash_node_t *prev, ab_hash_node_t *n, ui32_t hashnr)
{
    ab_hash_node_t * next = NULL;
    ab_hash_node_t * node = ABA_HNODE(n);
    ab_hash_node_t * prev_node = NULL;
    bool delete_owner = false;

    //find prev node
    if (!prev) prev = ab_hash_find_node(hash, entry, hashnr, n);
    if (!prev) return false;

    //set delete flag
    for (;;)
    {
        next = node->next;
        if ((ui64_t)next&1) return false; //other thread is deleting this node
        if (OS_CAS(&node->next, PTR_P(next), PTR_D(next))) break;
    }

    //set link
    while(prev)
    {
        ab_hash_node_t * prev_node = ABA_HNODE(prev);
        if (OS_CAS(&prev_node->next, PTR_P(n), PTR_P(node->next))) return true;
        prev = ab_hash_find_node(hash, entry, hashnr, n);
    }

    return false;
}

static ab_hash_node_t *ab_hash_new_node(ab_hash_t *hash, ui32_t h, void *key, ui8_t key_len, void *value)
{
    ab_hash_node_t *node = NULL;
    ab_hash_node_t *n = (ab_hash_node_t *)ab_mem_alloc(hash->mem);
    if (!n)
    {
        return NULL;
    }
    node = ABA_HNODE(n);
    memset(node, 0, sizeof(ab_hash_node_t));
    node->hashnr = ab_reverse_bits(h) | 1;
    node->is_dummy = 0;
    node->key = key;
    node->key_len = key_len;
    node->value = value;
    node->next = NULL;
    return n;
}

static ab_hash_node_t *ab_hash_new_dummy(ab_hash_t *hash, ui32_t bucket)
{
    ab_hash_node_t *dummy = NULL;
    ab_hash_node_t *d = (ab_hash_node_t*)ab_mem_alloc(hash->mem);
    if (!d)
    {
        return NULL;
    }
    dummy = ABA_HNODE(d);
    memset(dummy, 0, sizeof(ab_hash_node_t));
    dummy->hashnr = ab_reverse_bits(bucket) | 0;
    dummy->is_dummy = 1;
    dummy->key = dummy->value = NULL;
    dummy->key_len = 0;
    dummy->next = NULL;

    return d;
}

static ab_hash_node_t *ab_hash_entry_insert(ab_hash_t *hash, ui32_t bucket)
{
    ui32_t offset = bucket%ENTRYS_PER_BLOCK;
    ui32_t block = bucket/ENTRYS_PER_BLOCK;
    ab_entry_block_t *entry_block = NULL;
    
    for(;;)
    {
        entry_block = hash->entry_blocks[block];
        if (entry_block == NULL)
        {
            ab_entry_block_t *new_block = (ab_entry_block_t *)ab_malloc(sizeof(ab_entry_block_t));
            memset(new_block, 0, sizeof(ab_entry_block_t));
            if (!OS_CAS(&hash->entry_blocks[block], NULL, new_block)) 
            {
                //other thread has seted
                ab_free(new_block);
                continue;
            }
        }
        else break;
    }
    
    if (entry_block->entrys[offset] == NULL)
    {
        ui32_t dummy_hashnr = 0;
        ab_hash_node_t *d = ab_hash_new_dummy(hash, bucket);
        if (!d) return NULL;
        dummy_hashnr = ABA_HNODE(d)->hashnr;

        //the first
        if (bucket == 0) 
        {
            entry_block->entrys[offset] = hash->list_head;
            return entry_block->entrys[offset];
        }

        //get parent entry
        ui32_t parent = ab_clear_highest_bit(bucket);
        ab_hash_node_t *p_entry = ab_hash_entry_insert(hash, parent);

        //insert to list begin at p_entry
        ab_hash_list_insert(hash, p_entry, d, dummy_hashnr);

        if (!OS_CAS(&entry_block->entrys[offset], NULL, d)) 
        {
            //other thread has seted
            ab_hash_list_delete(hash, p_entry, NULL, d, dummy_hashnr);
        }
    }
    
    return entry_block->entrys[offset];
}

static ab_hash_node_t *ab_hash_entry_get(ab_hash_t *hash, ui32_t bucket)
{
    ui32_t offset = bucket%ENTRYS_PER_BLOCK;
    ui32_t block = bucket/ENTRYS_PER_BLOCK;
    ab_entry_block_t *entry_block = NULL;
    ui32_t parent = 0;
    
    entry_block = hash->entry_blocks[block];
    if (entry_block == NULL || entry_block->entrys[offset] == NULL)
    {
        if (bucket == 0) return NULL;
        parent = ab_clear_highest_bit(bucket);
        return ab_hash_entry_get(hash, parent);
    }
    
    return entry_block->entrys[offset];
}

//copy from level db
static ui32_t hashfunc(ui32_t seed, const void *key, int len) 
{
    /* 'm' and 'r' are mixing constants generated offline.
     They're not really 'magic', they just happen to work well.  */
    const ui32_t m = 0x5bd1e995;
    const int r = 24;
 
    /* Initialize the hash to a 'random' value */
    ui32_t h = seed ^ len;
 
    /* Mix 4 bytes at a time into the hash */
    const unsigned char *data = (const unsigned char *)key;
 
     /* key-len is 0, hash by key's address */
    if (len == 0) return (ui32_t)(size_t)key;
    
    while(len >= 4) {
        ui32_t k = *(ui32_t*)data;
 
        k *= m;
        k ^= k >> r;
        k *= m;
 
        h *= m;
        h ^= k;
 
        data += 4;
        len -= 4;
    }
 
    /* Handle the last few bytes of the input array  */
    switch(len) {
    case 3: h ^= data[2] << 16;
    case 2: h ^= data[1] << 8;
    case 1: h ^= data[0]; h *= m;
    };
 
    /* Do a few final mixes of the hash to ensure the last few
     * bytes are well-incorporated. */
    h ^= h >> 13;
    h *= m;
    h ^= h >> 15;
    h &= 0xEFFFFFFF;    
 
    return (ui32_t)h;
}



const ui8_t ab_bits_reverse_table[256] = 
{
    0x00, 0x80, 0x40, 0xC0, 0x20, 0xA0, 0x60, 0xE0, 0x10, 0x90, 0x50, 0xD0,
    0x30, 0xB0, 0x70, 0xF0, 0x08, 0x88, 0x48, 0xC8, 0x28, 0xA8, 0x68, 0xE8,
    0x18, 0x98, 0x58, 0xD8, 0x38, 0xB8, 0x78, 0xF8, 0x04, 0x84, 0x44, 0xC4,
    0x24, 0xA4, 0x64, 0xE4, 0x14, 0x94, 0x54, 0xD4, 0x34, 0xB4, 0x74, 0xF4,
    0x0C, 0x8C, 0x4C, 0xCC, 0x2C, 0xAC, 0x6C, 0xEC, 0x1C, 0x9C, 0x5C, 0xDC,
    0x3C, 0xBC, 0x7C, 0xFC, 0x02, 0x82, 0x42, 0xC2, 0x22, 0xA2, 0x62, 0xE2,
    0x12, 0x92, 0x52, 0xD2, 0x32, 0xB2, 0x72, 0xF2, 0x0A, 0x8A, 0x4A, 0xCA,
    0x2A, 0xAA, 0x6A, 0xEA, 0x1A, 0x9A, 0x5A, 0xDA, 0x3A, 0xBA, 0x7A, 0xFA,
    0x06, 0x86, 0x46, 0xC6, 0x26, 0xA6, 0x66, 0xE6, 0x16, 0x96, 0x56, 0xD6,
    0x36, 0xB6, 0x76, 0xF6, 0x0E, 0x8E, 0x4E, 0xCE, 0x2E, 0xAE, 0x6E, 0xEE,
    0x1E, 0x9E, 0x5E, 0xDE, 0x3E, 0xBE, 0x7E, 0xFE, 0x01, 0x81, 0x41, 0xC1,
    0x21, 0xA1, 0x61, 0xE1, 0x11, 0x91, 0x51, 0xD1, 0x31, 0xB1, 0x71, 0xF1,
    0x09, 0x89, 0x49, 0xC9, 0x29, 0xA9, 0x69, 0xE9, 0x19, 0x99, 0x59, 0xD9,
    0x39, 0xB9, 0x79, 0xF9, 0x05, 0x85, 0x45, 0xC5, 0x25, 0xA5, 0x65, 0xE5,
    0x15, 0x95, 0x55, 0xD5, 0x35, 0xB5, 0x75, 0xF5, 0x0D, 0x8D, 0x4D, 0xCD,
    0x2D, 0xAD, 0x6D, 0xED, 0x1D, 0x9D, 0x5D, 0xDD, 0x3D, 0xBD, 0x7D, 0xFD,
    0x03, 0x83, 0x43, 0xC3, 0x23, 0xA3, 0x63, 0xE3, 0x13, 0x93, 0x53, 0xD3,
    0x33, 0xB3, 0x73, 0xF3, 0x0B, 0x8B, 0x4B, 0xCB, 0x2B, 0xAB, 0x6B, 0xEB,
    0x1B, 0x9B, 0x5B, 0xDB, 0x3B, 0xBB, 0x7B, 0xFB, 0x07, 0x87, 0x47, 0xC7,
    0x27, 0xA7, 0x67, 0xE7, 0x17, 0x97, 0x57, 0xD7, 0x37, 0xB7, 0x77, 0xF7,
    0x0F, 0x8F, 0x4F, 0xCF, 0x2F, 0xAF, 0x6F, 0xEF, 0x1F, 0x9F, 0x5F, 0xDF,
    0x3F, 0xBF, 0x7F, 0xFF};

static inline ui32_t ab_reverse_bits(ui32_t key) 
{
    return (ab_bits_reverse_table[key & 255] << 24) |
            (ab_bits_reverse_table[(key >> 8) & 255] << 16) |
            (ab_bits_reverse_table[(key >> 16) & 255] << 8) |
            ab_bits_reverse_table[(key >> 24)];
}

static inline ui32_t ab_clear_highest_bit(ui32_t v) 
{
    ui32_t w = v >> 1;
    w |= w >> 1;
    w |= w >> 2;
    w |= w >> 4;
    w |= w >> 8;
    w |= w >> 16;
    return v & w;
}
