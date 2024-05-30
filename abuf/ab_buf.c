#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "ab_global.h"
#include "ab_mem.h"
#include "ab_hash.h"
#include "ab_skiplist.h"
#include "ab_list.h"
#include "ab_buf.h"
#include "ab_json.h"


#define ABA_MEMP(p)         ((ab_val_info_t*)AB_ABA_P(ab_mem_item_t,p)->mem);
 
typedef struct
{
    ui64_t updating;
    ui64_t incr[AB_MAX_INDEX];
} ab_val_info_t;

#define ab_val_info_size(buf)  (sizeof(ui64_t) + (buf)->index_num * sizeof(ui64_t))
#define ab_buf_val_ptr(buf, v) ((void *)&v->incr[buf->index_num])

static int ab_buf_cmp(void *k1, void *k2, void *param);
static int ab_buf_print(void *k, void *v, void *param);
static ui32_t ab_buf_index_from_value(ab_buf_t *buf, int id, ab_val_info_t *abv, void **containter, void **key, ui8_t *key_len);
static ui32_t ab_buf_index_from_key(ab_buf_t *buf, int id, void **containter, void *key, ui8_t *key_len);

static inline ab_index_rf_t * ab_rf(int index_type)
{
    switch (index_type)
    {
    case index_uniq_hash:     return &ab_index_rf_hash; break;
    case index_uniq_skiplist: return &ab_index_rf_skiplist_uniq; break;
    case index_dupl_skiplist: return &ab_index_rf_skiplist_dupl; break;
    case index_list:          return &ab_index_rf_list; break;
    default: return NULL; break;
    }
}

static inline int ab_buf_val_len(ab_buf_t *buf, void *val)
{
    int val_len = 0;

    //fix-size
    if (buf->struct_size == buf->base_size) val_len = buf->struct_size;
    //var-size
    else val_len = buf->base_size + ab_jsbin_len((ui8_t*)val + buf->base_size);

    return val_len;
}

int ab_buf_init_func(ab_buf_t *buf, ui32_t struct_size, ui32_t base_size)
{
    memset(buf, 0, sizeof(ab_buf_t));

    buf->struct_size = struct_size;
    buf->base_size = base_size;

    // because buf->index_num is not set before attach index
    // the min-size set to base_size
    // the max-size set to sizeof(ab_val_info_t)+struct_size
    buf->mpool = ab_mpool_init(base_size, sizeof(ab_val_info_t)+struct_size);
    if (!buf->mpool)
    {
        return ab_ret_outofmemory;
    }
    return 0;
}

int ab_buf_attach_index_func(ab_buf_t *buf, ab_key_info_t *key_info)
{
    int id = buf->index_num;
    int index_type = key_info->index_type;

    if (id >= AB_MAX_INDEX) 
        return ab_return(ab_ret_too_many_indexs);

    if (index_type >= index_max)
        return ab_return(ab_ret_invalid_index_type);

    buf->containter[id] = ab_rf(key_info->index_type)->init(ab_attr_threadsafe, ab_buf_cmp, ab_buf_print, &buf->attr[id]);
    if (buf->containter[id] == NULL)
    {
        ab_buf_destory(buf);
        return -1;
    } 

    //append key-attr
    buf->attr[id] = *key_info;
    buf->index_num++;

    return 0;
}

void ab_buf_destory(ab_buf_t *buf)
{
    int i = 0;

    for (i=0; i<buf->index_num; i++)
    {
        void *containter = buf->containter[i];
        int index_type = buf->attr[i].index_type;
        if (containter) ab_rf(index_type)->destory(containter);
    }

    if (buf->mpool)
    {
        ab_mpool_destory(buf->mpool);
        buf->mpool = NULL;
    }
}

int ab_buf_put(ab_buf_t *buf, void *value)
{
    int i = 0;
    void *key = NULL;
    ui8_t key_len = 0;
    ab_val_info_t *abv = NULL; 
    ab_mem_item_t *mp_ptr = NULL;
    int val_len = ab_buf_val_len(buf, value);
    int val_size = ab_val_info_size(buf) + val_len;
    ui16_t mem_id = MEM_ID_FROM_SIZE(val_size);

    ab_mpool_epoch_enter(buf->mpool, mem_id);
    
    //new value
    if ((mp_ptr = ab_mpool_alloc(buf->mpool, val_size)) == NULL) return -1;
    abv = ABA_MEMP(mp_ptr);

    memcpy(ab_buf_val_ptr(buf,abv), value, val_len);

    //abv->updating = 1;
    abv->updating = 0;
    OS_ATOMIC_ADD(&abv->updating, 1);

    //insert all index
    for (i=0; i<buf->index_num; i++)
    {
        void *containter = NULL;
        ui32_t index_type = ab_buf_index_from_value(buf, i, abv, &containter, &key, &key_len);
        if (!ab_rf(index_type)->put(containter, key, key_len, mp_ptr, &abv->incr[i]))
        {
            //rollback and delete all keys has inserted
            int x = 0;
            for (x=0; x<i; x++) 
            {
                index_type = ab_buf_index_from_value(buf, x, abv, &containter, &key, &key_len);
                ab_rf(index_type)->del(containter, key, key_len, abv->incr[x]);
            }

            //abv->updating = 0;
            OS_ATOMIC_SUB(&abv->updating, 1);
            ab_mpool_free(buf->mpool, mp_ptr);
            ab_mpool_epoch_leave(buf->mpool);
            return -2;
        }
    }

    //abv->updating = 0;
    OS_ATOMIC_SUB(&abv->updating, 1);

    ab_mpool_epoch_leave(buf->mpool);
    return 0;
}

int ab_buf_get(ab_buf_t *buf, void *key, int key_id, void *value, int top)
{
    void *containter = NULL;
    ab_val_info_t *abv = NULL;
    ab_mem_item_t *mp_ptr = NULL;
    ui32_t index_type = 0;
    ui8_t *vp = value;
    ui64_t incr = 0;
    ui8_t key_len = 0;
    int i = 0;
    int val_len = 0;

    if (key_id >= buf->index_num) return -1;
    index_type = ab_buf_index_from_key(buf, key_id, &containter, key, &key_len);

    ab_mpool_epoch_enter(buf->mpool, 0);

    for (i=0; i<top; i++)
    {
        mp_ptr = ab_rf(index_type)->get(containter, key, key_len, incr);
        if (!mp_ptr) break;
        abv = ABA_MEMP(mp_ptr);
        
        //copy value
        val_len = ab_buf_val_len(buf, ab_buf_val_ptr(buf,abv));
        memcpy(vp, ab_buf_val_ptr(buf,abv), val_len);

        //next incr
        incr = abv->incr[key_id] + 1;

        //next vp
        vp += buf->struct_size;
    }

    ab_mpool_epoch_leave(buf->mpool);
    return i;
}

static int ab_buf_del_one(ab_buf_t *buf, void *key, int key_id, void *value)
{
    int i = 0;
    int ret = 0;
    void *containter = NULL;
    ab_val_info_t *abv = NULL;
    ab_mem_item_t *mp_ptr = NULL;
    ui32_t index_type = 0;
    ab_mem_item_t *ab_value[AB_MAX_INDEX] = {0};
    void *sub_key = NULL;
    int all_deleted = 1;
    ui8_t key_len = 0;
    int val_len = 0;

    if (key_id >= buf->index_num) return -1;

    ab_mpool_epoch_enter(buf->mpool, 0);

    index_type = ab_buf_index_from_key(buf, key_id, &containter, key, &key_len);
    mp_ptr = ab_rf(index_type)->get(containter, key, key_len, 0);
    if (!mp_ptr)
    {
        ab_mpool_epoch_leave(buf->mpool);
        return -2;
    }
    abv = ABA_MEMP(mp_ptr);

    //check updating counter
    //OS_ATOMIC_ADD(&abv->updating, 1);
    //if (!OS_CAS(&abv->updating, 1, 2))
    //{
    //    OS_ATOMIC_SUB(&abv->updating, 1);
    //    return 1;   //>0 value is updating, need retry
    //}
    if (!OS_CAS(&abv->updating, 0, 1)) return 1;
    
    //delete all indexs
    for (i=0; i < buf->index_num; i++)
    {
        index_type = ab_buf_index_from_value(buf, i, abv, &containter, &sub_key, &key_len);
        ab_value[i] = ab_rf(index_type)->del(containter, sub_key, key_len, abv->incr[i]);
        if (ab_value[i] == NULL) all_deleted = 0;
    }

    assert(all_deleted == 1);

    //copy value
    val_len = ab_buf_val_len(buf, ab_buf_val_ptr(buf,abv));
    if (value) memcpy(value, ab_buf_val_ptr(buf,abv), val_len);

    //free value
    ab_mpool_free(buf->mpool, mp_ptr);

    ab_mpool_epoch_leave(buf->mpool);
    return 0;
}

int ab_buf_del(ab_buf_t *buf, void *key, int key_id, void *value, int top)
{
    int i = 0;
    int del_ret = 0;
    ui8_t *vp = value;

    while(i < top)
    {
        del_ret = ab_buf_del_one(buf, key, key_id, vp);
        if (del_ret < 0) break;      //key not found!
        if (del_ret > 0) continue;   //value deleted by other thread, need retry

        if (value) vp += buf->struct_size;
        i++;
    }

    return i;
}

bool ab_buf_is_empty(ab_buf_t *buf)
{
    return ab_rf(buf->attr[0].index_type)->is_empty(buf->containter[0]);
}

long ab_buf_num(ab_buf_t *buf)
{
    return (long)ab_rf(buf->attr[0].index_type)->num(buf->containter[0]);
}

static inline int ab_key_len(void *key, ab_key_info_t *abk)
{
    if (abk->key_type[0] == type_num[0]) return abk->key_size;
    else if (abk->key_type[0] == type_str[0]) return strnlen(key, abk->key_size);
    else if (abk->key_type[0] == type_jspath[0]) return ((ab_jsbin_t*)key)->jsbin_len;
    else return 0;
}


static ui32_t ab_buf_index_from_value(ab_buf_t *buf, int id, ab_val_info_t *abv, void **containter, void **key, ui8_t *key_len)
{
    *containter = buf->containter[id];
    *key = (char*)ab_buf_val_ptr(buf,abv) + buf->attr[id].key_offset;
    *key_len = ab_key_len(*key, &buf->attr[id]);

    return buf->attr[id].index_type;
}

static ui32_t ab_buf_index_from_key(ab_buf_t *buf, int id, void **containter, void *key, ui8_t *key_len)
{
    *containter = buf->containter[id];
    *key_len = ab_key_len(key, &buf->attr[id]);

    return buf->attr[id].index_type;
}


/*
* compare functions
*/

static int ab_buf_cmp_ui(void *k1, void *k2, int len)
{
    switch (len)
    {
    case 1: return *(unsigned char *)k1 - *(unsigned char *)k2;
    case 2: return *(unsigned short *)k1 - *(unsigned short *)k2;
    case 4: return *(unsigned int *)k1 - *(unsigned int *)k2;
    case 8: return *(unsigned long long *)k1 - *(unsigned long long *)k2;
    default: return 0;
    }
}

static int ab_buf_cmp_str(void *k1, int len1, void *k2, int len2)
{
    char *p1 = (char *)k1;
    char *p2 = (char *)k2;
    char *end1 = p1 + len1;
    char *end2 = p2 + len2;

    while(p1 < end1 && p2 < end2 && *p1 && *p2 && *p1 == *p2) 
    {
        p1++;
        p2++;
    }
    if (p1 < end1 && p2 < end2) return (*p1 - *p2);
    else if (p1 < end1) return *p1;
    else if (p2 < end2) return -(*p2);
    else return 0;  
}

ui8_t* ab_jsbuf_get_varint64(ui8_t* p, ui64_t* value);
static int ab_buf_cmp(void *k1, void *k2, void *param)
{
    ab_key_info_t *key = (ab_key_info_t *)param;

    if (key->key_type[0] == type_num[0]) return ab_buf_cmp_ui(k1, k2, key->key_size);
    else if (key->key_type[0] == type_str[0]) return ab_buf_cmp_str(k1, key->key_size, k2, key->key_size);
    else if (key->key_type[0] == type_jspath[0])
    {
        js_item_t set1 = {0};
        int set_num1 = 0;
        ui64_t lv1 = 0;

        js_item_t set2 = {0};
        int set_num2 = 0;
        ui64_t lv2 = 0;
        
        ab_json_parse_path(k1, key->key_type, &set1, 1, &set_num1);
        ab_json_parse_path(k2, key->key_type, &set2, 1, &set_num2);

        //get fail
        if (set_num1 != 1 || set_num2 != 1) 
        {
            return 1;
        }

        //not base type or diffrent type
        if ((set1.type == ABJSION_ARRAY || set1.type == ABJSION_OBJECT) ||
            (set2.type == ABJSION_ARRAY || set2.type == ABJSION_OBJECT) ||
            (set1.type != set2.type))
        {
            return 1;
        }

        //cmp base type
        if (set1.type == ABJSION_STRING)
        {
            return ab_buf_cmp_str(set1.value, set1.value_len, set2.value, set2.value_len);
        }

        ab_jsbuf_get_varint64(set1.value, &lv1);
        ab_jsbuf_get_varint64(set2.value, &lv2);
        
        if (lv1 == lv2) return 0;
        else if (lv1 > lv2) return 1;
        else return -1;
    }
    else return 0;
}

static int ab_buf_print(void *k, void *v, void *param)
{
    ab_key_info_t *key = (ab_key_info_t *)param;

    if (key->key_type[0] == type_num[0]) 
    {
        if(key->key_size == 1) printf("%u ", *(unsigned char *)k);
        if(key->key_size == 2) printf("%u ", *(unsigned short *)k);
        if(key->key_size == 4) printf("%u ", *(unsigned int *)k);
        if(key->key_size == 8) printf("%llu ", *(unsigned long long *)k);
    }
    else if (key->key_type[0] == type_str[0]) 
    {
        printf("%s ", (char *)k);
    }
    return 0;
}

