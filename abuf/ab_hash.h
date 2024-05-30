
#ifndef __AB_HASH__
#define __AB_HASH__

#include <stdbool.h>
#include "ab_global.h"
#include "ab_mem.h"
#include "ab_index.h"

#define ENTRYS_PER_BLOCK   4094
#define BUCKET_MAX         (ENTRYS_PER_BLOCK*ENTRYS_PER_BLOCK)

typedef struct ab_hash_node
{
    void *key;
    void *value;
    ui32_t hashnr;
    ui8_t is_dummy;
    ui8_t key_len; 
    struct ab_hash_node *next;
}ab_hash_node_t;

typedef struct ab_entry_block
{
    ab_hash_node_t *entrys[ENTRYS_PER_BLOCK];
}ab_entry_block_t;

typedef struct ab_hash
{
    ui64_t num;
    ui64_t size;
    ui64_t maxload;
    ab_entry_block_t *entry_blocks[ENTRYS_PER_BLOCK];
    ab_hash_node_t *list_head;
    ab_mem_t *mem;

    ab_compare_f compare;
    void *param;
}ab_hash_t;

ab_hash_t *ab_hash_init(ui32_t attr, ab_compare_f compare, ab_print_f print, void *param);
void ab_hash_destory(ab_hash_t *hash);
bool ab_hash_insert(ab_hash_t *hash, void *key, ui8_t keylen, void *value, ui64_t *incr);
void *ab_hash_get(ab_hash_t *hash, void *key, ui8_t keylen, ui64_t incr);
void *ab_hash_delete(ab_hash_t *hash, void *key, ui8_t keylen, ui64_t incr);
ui64_t ab_hash_num(ab_hash_t *hash);
bool ab_hash_is_empty(ab_hash_t *hash);
void ab_hash_foreach(ab_hash_t *hash, void (* callback)(void *key, ui32_t hashnr, void *param), void *param);

extern ab_index_rf_t ab_index_rf_hash;

#endif
