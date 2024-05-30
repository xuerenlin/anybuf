#ifndef __AB_BUF_H__
#define __AB_BUF_H__

#include <stdbool.h>

//index type
enum ab_index_type 
{
    index_uniq_hash         = 0,
    index_uniq_skiplist     = 1,
    index_dupl_skiplist     = 2,
    index_list              = 3,
    index_max
};

//ab_buf
#define type_num      "n"
#define type_str      "s"
#define type_jspath   "$"
#define type_null     ""

#define ab_attr_init(st,field,key_type,index_type) \
    {sizeof(st), (int)(long)&((st*)0)->field, sizeof(((st*)0)->field), key_type, index_type}

#define AB_MAX_INDEX 4

typedef struct
{
    int struct_size;
    int key_offset;
    int key_size;
    const char *key_type;
    int index_type;
} ab_key_info_t; 

typedef struct 
{
    ab_key_info_t attr[AB_MAX_INDEX];
    void * containter[AB_MAX_INDEX];
    int struct_size;
    int base_size;
    int index_num;
    void *mpool;
} ab_buf_t;

int ab_buf_attach_index_func(ab_buf_t *buf, ab_key_info_t *key_info);

#define ab_buf_attach_pk(buf,st,field,key_type)  do{\
    ab_key_info_t key_info = ab_attr_init(st,field,key_type,index_uniq_hash); \
    ab_buf_attach_index_func(buf, &key_info); \
}while(0)

#define ab_buf_attach_index(buf,st,field,key_type)  do{\
    ab_key_info_t key_info = ab_attr_init(st,field,key_type,index_dupl_skiplist); \
    ab_buf_attach_index_func(buf, &key_info); \
}while(0)

#define ab_buf_attach_list(buf,st,field)  do{\
    ab_key_info_t key_info = ab_attr_init(st,field,type_null,index_list); \
    ab_buf_attach_index_func(buf, &key_info); \
}while(0)

#define ab_buf_init_fix(buf,st) ab_buf_init_func(buf, sizeof(st), sizeof(st))
#define ab_buf_init_var(buf,st,field) ab_buf_init_func(buf, sizeof(st), (int)(long)&((st*)0)->field)

int ab_buf_init_func(ab_buf_t *buf, unsigned int struct_size, unsigned int base_size);
void ab_buf_destory(ab_buf_t *buf);
int ab_buf_put(ab_buf_t *buf, void *value);
int ab_buf_get(ab_buf_t *buf, void *key, int key_id, void *value, int top);
int ab_buf_del(ab_buf_t *buf, void *key, int key_id, void *value, int top);
bool ab_buf_is_empty(ab_buf_t *buf);
long ab_buf_num(ab_buf_t *buf);

#endif
