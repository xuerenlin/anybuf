#ifndef __onefile_ab_global__
#define __onefile_ab_global__

typedef unsigned char       ui8_t;
typedef unsigned short      ui16_t;
typedef unsigned int        ui32_t;
typedef int                 i32_t;
typedef unsigned long long  ui64_t;

#define _min(a,b)  ((a)>(b)?(b):(a))

#endif

#ifndef __onefile_ab_error__
#define __onefile_ab_error__

typedef enum
{
    ab_ret_ok                                        = 0,
    ab_ret_outofmemory                               = 1,
    ab_ret_invalid_ptr                               = 2,
    ab_ret_too_many_indexs                           = 3,
    ab_ret_invalid_index_type                        = 4,
    ab_ret_from_value_nochange                       = 5,
    ab_ret_skiplist_dupkey                           = 101,
    ab_ret_skiplist_norecord                         = 102,
    ab_ret_skiplist_updating                         = 103,
    ab_ret_json_invalid_unicode                      = 201,
    ab_ret_json_invalid_escape                       = 202,
    ab_ret_json_invalid_number                       = 203,
    ab_ret_json_invalid_syntax                       = 204,
    ab_ret_json_invalid_type                         = 205,
    ab_ret_json_be_end                               = 206,
    ab_ret_json_expect_string                        = 207,
    ab_ret_json_expect_colon                         = 208,
    ab_ret_json_expect_brackets_a                    = 209, 
    ab_ret_json_expect_brackets_o                    = 210,
    ab_ret_jsonpath_invalid_string_token             = 301,
    ab_ret_jsonpath_expected_root_or_current_symbol  = 302,
    ab_ret_jsonpath_expected_tail_operator           = 303,
    ab_ret_jsonpath_expected_number_wildcard_or_union= 304,
    ab_ret_jsonpath_expected_right_bracket           = 305,
    ab_ret_jsonpath_invalid_array_slice_number       = 306,
    ab_ret_jsonpath_expected_array_slice_symbol      = 307,
    ab_ret_jsonpath_expected_identifier              = 308,
    ab_ret_jsonpath_expected_expression              = 309,     
    ab_ret_jsonpath_expected_value                   = 310,
    ab_ret_jsonpath_expected_left_bracket            = 311,
    ab_ret_jsonpath_expected_end_of_expression       = 312,
    ab_ret_jsonpath_expected_recursive_operator      = 313,
    ab_ret_jsonpath_expected_filter_operator         = 314,
    ab_ret_jsonpath_type_conflict_long               = 315,
    ab_ret_jsonpath_type_conflict_string             = 316,
    ab_ret_jsonpath_item_not_found                   = 317,
    ab_ret_jsonpath_parent_item_lost                 = 318,

    ab_ret_jsonbuf_no_left_space                     = 401,
    ab_ret_jsonbuf_no_left_content                   = 402,

} ab_ret_t;

int ab_return(int err);
int ab_return_ex(int err, const char *exinfo);

const char *ab_error_desc(ab_ret_t ret);
const char *ab_last_error();
void ab_print_last_error();

#endif

#ifndef __onefile_ab_os__
#define __onefile_ab_os__

#define MAX_THREAD_NUM 100

#define CPU_BARRIER()       __asm__ __volatile__("mfence": : :"memory")
#define OS_CAS(p,o,n)       __sync_bool_compare_and_swap((p),(o),(n))
#define OS_ATOMIC_ADD(p,a)  __sync_fetch_and_add((p),(a));
#define OS_ATOMIC_SUB(p,a)  __sync_fetch_and_sub((p),(a));

int ab_get_thread_id();
int ab_get_pagesize();

#endif

#ifndef __onefile_ab_index__
#define __onefile_ab_index__

#include <stdbool.h>


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

#ifndef __onefile_ab_mem__
#define __onefile_ab_mem__

#include <stddef.h>



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

#ifndef __onefile_ab_list__
#define __onefile_ab_list__

#include <stdbool.h>



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


#ifndef __onefile_ab_hash__
#define __onefile_ab_hash__

#include <stdbool.h>




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

#ifndef __onefile_ab_skiplist__
#define __onefile_ab_skiplist__

#include <stdbool.h>




#define AB_SKIPLIST_MAX_HEIGH  16

#define ab_sl_is_duplicate(attr)    ((attr)&ab_attr_duplicate)
#define ab_sl_is_threadsafe(attr)   ((attr)&ab_attr_threadsafe)

typedef union ab_sln_info_t
{
    struct 
    {
        ui64_t is_marker    : 1;
        ui64_t is_deleting  : 1;
        ui64_t is_inserting : 1;
        ui64_t height       : 5;
        ui64_t incr_key     : 56;
    } info;
    ui64_t ui64;
}ab_sln_info_t;

typedef struct ab_skipnode_t
{
    void *key;
    void *value;
    ab_sln_info_t info;
    struct ab_skipnode_t *next[0];
} ab_skipnode_t;

typedef struct ab_skiplist_t
{ 
    ui32_t attr;
    ui32_t height;      //use ui32_t type but not ui8_t for CAS
    ui64_t incr_key;
    ui64_t num;
    ab_skipnode_t *head;
    ab_compare_f compare;
    ab_print_f print;
    void * param;

    ab_mem_t *mem1;
    ab_mem_t *mem2;
    ab_mem_t *level_mem[AB_SKIPLIST_MAX_HEIGH+1];
} ab_skiplist_t;

ab_skiplist_t *ab_skiplist_init(ui32_t attr, ab_compare_f compare, ab_print_f print, void *param);
void ab_skiplist_destory(ab_skiplist_t *list);

ab_ret_t ab_skiplist_insert(ab_skiplist_t *list, void *key, void *value, ui64_t *incr);
void *ab_skiplist_delete(ab_skiplist_t *list, void *key, ui64_t incr);
void *ab_skiplist_get(ab_skiplist_t *list, void *key, ui64_t incr);
bool ab_skiplist_is_empty(ab_skiplist_t *list);
ui64_t ab_skiplist_num(ab_skiplist_t *list);
void ab_skiplist_print(ab_skiplist_t *list);

ab_index_rf_t ab_index_rf_skiplist_uniq;
ab_index_rf_t ab_index_rf_skiplist_dupl;


#endif

#ifndef __onefile_ab_json__
#define __onefile_ab_json__

#include <stdio.h>

typedef enum 
{
    ABJSION_INVALID = 0,
    ABJSION_FALSE   = 1,
    ABJSION_TRUE    = 2,
    ABJSION_NULL    = 3,
    ABJSION_ULONG   = 4,
    ABJSION_DOUBLE  = 5,
    ABJSION_STRING  = 6,
    ABJSION_ARRAY   = 7,
    ABJSION_OBJECT  = 8,
    ABJSION_UNKNOW  = 99,
} ab_json_type;

typedef enum ab_token_type_t
{
    TOK_INVALID,    //invalid input
    TOK_FALSE,      //false
    TOK_TRUE,       //true
    TOK_NULL,       //null
    TOK_NUMBER,     //0-9
    TOK_STRING,     //"*"
    TOK_ARRAY_B,    //[
    TOK_ARRAY_E,    //]
    TOK_OBJECT_B,   //{
    TOK_OBJECT_E,   //}
    TOK_COMMA,      //,
    TOK_COLON,      //:
    TOK_END,        //end of input
} ab_token_type_t;

#define IS_VALUE_TOKEN(type)  ((type) != TOK_INVALID && \
                               (type) != TOK_ARRAY_E && \
                               (type) != TOK_OBJECT_E && \
                               (type) != TOK_COMMA && \
                               (type) != TOK_COLON && \
                               (type) != TOK_END)

typedef struct ab_input_t
{
    int len;
    int left;
    const char *buffer;
    const char *current;
} ab_input_t;

typedef struct ab_token_t
{
    ab_token_type_t type;
    const char *begin;
    int len;
} ab_token_t;

typedef struct 
{
    int size;
    unsigned char *buf;
    unsigned char *current;
} ab_jsbuf_t;

typedef struct 
{
    unsigned char type;
    unsigned int name_len;
    unsigned char *name;
    unsigned int value_len;
    unsigned char *value;
    unsigned char *orgbuf;
} js_item_t;

typedef struct 
{
    int jsbin_len;
    int buf_size;
    unsigned char buf[];
} ab_jsbin_t;

#define ab_jsbin_init(jsbin)  ((ab_jsbin_t*)(jsbin))->jsbin_len = 0; \
    ((ab_jsbin_t*)(jsbin))->buf_size = sizeof(jsbin)-sizeof(ab_jsbin_t);

#define ab_jsbin_len(jsbin)   (sizeof(ab_jsbin_t) + _min(((ab_jsbin_t*)(jsbin))->jsbin_len,((ab_jsbin_t*)(jsbin))->buf_size));

int ab_json_parse(const char *str, int len, unsigned char *jsbin);
void ab_json_print(unsigned char *jsbin, int is_format);
void ab_json_snprint(unsigned char *jsbin, int is_format, char *out_buf, int size);

int ab_json_parse_path(unsigned char *jsbin, const char* path, js_item_t *set, int set_max, int *set_num);
int ab_json_from_long(unsigned char *jsbin, const char* path, unsigned long long v);
int ab_json_from_string(unsigned char *jsbin, const char* path, const char* v);
int ab_json_to_long(unsigned char *jsbin, const char* path, unsigned long long *v);
int ab_json_to_string(unsigned char *jsbin, const char* path, char* v, int v_size);

#endif

#ifndef __onefile_ab_buf__
#define __onefile_ab_buf__

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

#include <stdio.h>
#include <string.h>



char g_last_error[MAX_THREAD_NUM][512] = {0};

const char *ab_error_desc(ab_ret_t ret)
{
    switch (ret)
    {
    case ab_ret_ok:                                         return "ok";
    case ab_ret_outofmemory:                                return "out of memory";
    case ab_ret_invalid_ptr:                                return "invalid/null pointer";
    case ab_ret_too_many_indexs:                            return "too many indexs";
    case ab_ret_invalid_index_type:                         return "invalid index type";
    case ab_ret_from_value_nochange:                        return "from value set function return value no changed";
    case ab_ret_skiplist_dupkey:                            return "skiplist fail: duplicate key";
    case ab_ret_skiplist_norecord:                          return "skiplist fail: no record";
    case ab_ret_skiplist_updating:                          return "skiplist fail: item is updating";
    case ab_ret_json_invalid_unicode:                       return "parse json fail: encountered illegal unicode characters";
    case ab_ret_json_invalid_escape:                        return "parse json fail: encountered illegal escape character";
    case ab_ret_json_invalid_number:                        return "parse json fail: encountered illegal number, maybe number is too big";
    case ab_ret_json_invalid_syntax:                        return "parse json fail: unexpected character";
    case ab_ret_json_invalid_type:                          return "parse json fail: encountered illegal type";
    case ab_ret_json_be_end:                                return "parse json fail: extra characters at the end";
    case ab_ret_json_expect_string:                         return "parse json fail: expected string";
    case ab_ret_json_expect_colon:                          return "parse json fail: expected colon ':' ";
    case ab_ret_json_expect_brackets_a:                     return "parse json fail: expected brackets ']' ";
    case ab_ret_json_expect_brackets_o:                     return "parse json fail: expected brackets '}' ";
    case ab_ret_jsonpath_invalid_string_token:              return "parse jsonpath fail: invalid string token";
    case ab_ret_jsonpath_expected_root_or_current_symbol:   return "parse jsonpath fail: expected root or current symbol $ @";
    case ab_ret_jsonpath_expected_tail_operator:            return "parse jsonpath fail: expected child, recursive descent, subscript, filter or array slice operator . .. []";
    case ab_ret_jsonpath_expected_number_wildcard_or_union: return "parse jsonpath fail: expected number wildcar or union";
    case ab_ret_jsonpath_expected_right_bracket:            return "parse jsonpath fail: expected right bracket";
    case ab_ret_jsonpath_invalid_array_slice_number:        return "parse jsonpath fail: invalid array slice number";
    case ab_ret_jsonpath_expected_array_slice_symbol:       return "parse jsonpath fail: invalid array slice symbol";
    case ab_ret_jsonpath_expected_identifier:               return "parse jsonpath fail: expected identifier";
    case ab_ret_jsonpath_expected_expression:               return "parse jsonpath fail: expected ( expression )";     
    case ab_ret_jsonpath_expected_value:                    return "parse jsonpath fail: syntax error, expected a value";
    case ab_ret_jsonpath_expected_left_bracket:             return "parse jsonpath fail: expected left bracket [";
    case ab_ret_jsonpath_expected_end_of_expression:        return "parse jsonpath fail: expected end of expression )";
    case ab_ret_jsonpath_expected_recursive_operator:       return "parse jsonpath fail: expected recursive operator ..";
    case ab_ret_jsonpath_expected_filter_operator:          return "parse jsonpath fail: expected filter operator ?";
    case ab_ret_jsonpath_type_conflict_long:                return "set json fail: type conflict, can not set to a long value";
    case ab_ret_jsonpath_type_conflict_string:              return "set json fail: type conflict, can not set to a string value";
    case ab_ret_jsonpath_item_not_found:                    return "parse jsonpath fail: item not found";
    case ab_ret_jsonpath_parent_item_lost:                  return "set json fail: get parent item fail";

    case ab_ret_jsonbuf_no_left_space:                      return "parse json fail: jsonbuf no left space";
    case ab_ret_jsonbuf_no_left_content:                    return "parse print fail: jsonbuf no left content";
    default: return "unknow error";
    }
}

int ab_return(int err)
{
    int tid = ab_get_thread_id();
    if (err == 0) g_last_error[tid][0] = '\0';
    else snprintf(g_last_error[tid], sizeof(g_last_error[tid])-1, "any-buf fail(%d), %s", err, ab_error_desc(err));
    return err;
}

int ab_return_ex(int err, const char *exinfo)
{
    int tid = ab_get_thread_id();
    if (err == 0) g_last_error[tid][0] = '\0';
    else snprintf(g_last_error[tid], sizeof(g_last_error[tid])-1, "any-buf fail(%d), %s, exinfo:%s", err, ab_error_desc(err), exinfo);
    return err;
}

const char *ab_last_error()
{
    int tid = ab_get_thread_id();
    return g_last_error[tid];
}

void ab_print_last_error()
{
    printf("%s\n", ab_last_error());
}

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/syscall.h>



static int  ab_os_gettid()
{
    return syscall(SYS_gettid);
}

static int  ab_os_getpid()
{
    return getpid();
}

static bool ab_os_thread_exsit(int os_tid)
{
    char proc_path[256] = {0};

    sprintf(proc_path, "/proc/%u/task/%u", ab_os_getpid(), os_tid);
    return (access(proc_path, 0) == 0);
}


static int g_thread_count = 0;
static int g_thread_os_tid[MAX_THREAD_NUM] = {0};

static int ab_search_thread_id()
{
    int i = 0;
    int os_cur_tid = ab_os_gettid();

    for (i=0; i<MAX_THREAD_NUM; i++)
    {
        int os_prev_id = g_thread_os_tid[i];
        if (!ab_os_thread_exsit(os_prev_id))    //this thread has exit
        {
            if(OS_CAS(&g_thread_os_tid[i], os_prev_id, os_cur_tid))
            {
                //fprintf(stdout, "threads %u-%u has exit, reuse it\n", i, os_prev_id);
                return i;
            }
        }
    }
    return -1;
}

int ab_get_thread_id() 
{
    static __thread int tid = -1;
    
    //new thread
    if (tid == -1) 
    {
        tid = OS_ATOMIC_ADD(&g_thread_count, 1);
        if (tid < MAX_THREAD_NUM) g_thread_os_tid[tid] = ab_os_gettid();
        else tid = ab_search_thread_id(tid);
    }

    if (tid < 0)
    {
        fprintf(stderr, "there is too many threads, max is %u\n", MAX_THREAD_NUM);
        exit(1);
    }

    return tid;
}

int ab_get_pagesize()
{
    return getpagesize();
}

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sys/time.h>


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

#include <stdio.h>
#include <stdbool.h>



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


#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>



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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>


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

#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <math.h>





ui32_t ab_jsbuf_encode_varint64(ui8_t* dst, ui64_t v);
ui8_t* ab_jsbuf_get_varint64(ui8_t* p, ui64_t* value);
ui8_t *ab_jsbuf_get_item(ab_jsbuf_t* jsbuf, js_item_t *item);
int ab_jsbuf_put_item(ab_jsbuf_t* jsbuf, ui8_t type, ui8_t *name, ui32_t name_len, ui8_t *value, ui32_t value_len);
static int ab_jsbuf_update_len(ab_jsbuf_t* jsbuf, ui8_t *pos, int diff_len);

static void ab_json_get_token(ab_input_t *input, ab_token_t *token);
static void ab_json_putback_token(ab_input_t *input, ab_token_t *token);
int ab_json_token_to_string(const char *begin, int len, char **str);

static unsigned char utf16_literal_to_utf8(const unsigned char * const input_pointer, const unsigned char * const input_end, unsigned char **output_pointer);
static ab_ret_t ab_json_parse_number(ab_input_t *input, ab_token_t *token, char *name, int name_len, ab_jsbuf_t *jsbuf);
static ab_ret_t ab_json_parse_string(ab_input_t *input, ab_token_t *token, char *name, int name_len, ab_jsbuf_t *jsbuf);
static ab_ret_t ab_json_parse_array(ab_input_t *input, char *name, int name_len, ab_jsbuf_t *jsbuf);
static ab_ret_t ab_json_parse_object(ab_input_t *input, char *name, int name_len, ab_jsbuf_t *jsbuf);
static ab_ret_t ab_json_parse_value(ab_input_t *input, char *name, int name_len, ab_jsbuf_t *jsbuf);

int ab_json_parse(const char *str, int len, ui8_t *jsbin)
{
    int ret = ab_ret_ok;
    unsigned char options = 0;
    char error_desc[1024] = {0};

    ab_input_t input = {0};
    ab_token_t token = {0};
    ab_jsbuf_t jsbuf = {0};
    
    //initralize input
    input.buffer = str;
    input.current = input.buffer;
    input.len = input.left = len;

    //initralize jsbuf
    jsbuf.buf = ((ab_jsbin_t*)jsbin)->buf;
    jsbuf.current = jsbuf.buf;
    jsbuf.size = ((ab_jsbin_t*)jsbin)->buf_size;

    ret = ab_json_parse_value(&input, "", 0, &jsbuf);
    if (ret == ab_ret_ok)
    {
        //check end
        ab_json_get_token(&input, &token);
        if (token.type != TOK_END)
        {
            ab_json_putback_token(&input, &token);
            ret = ab_ret_json_be_end;
        }
    }
    if (ret != ab_ret_ok)
    {
        sprintf(error_desc, "positon(offset:%u|%.10s...)", input.len-input.left, input.current);
        return ab_return_ex(ret, error_desc);
    }
    ((ab_jsbin_t*)jsbin)->jsbin_len = jsbuf.current - jsbuf.buf;
    return ab_ret_ok;
}


static ab_ret_t ab_json_parse_value(ab_input_t *input, char *name, int name_len, ab_jsbuf_t *jsbuf)
{
    ab_token_t token = {0};

    ab_json_get_token(input, &token);
    switch (token.type)
    {
    case TOK_FALSE:
        return ab_jsbuf_put_item(jsbuf, ABJSION_FALSE, name, name_len, NULL, 0);
        break;
    case TOK_TRUE:
        return ab_jsbuf_put_item(jsbuf, ABJSION_TRUE, name, name_len, NULL, 0);
        break;
    case TOK_NULL:
        return ab_jsbuf_put_item(jsbuf, ABJSION_NULL, name, name_len, NULL, 0);
        break;
    case TOK_STRING:
        return ab_json_parse_string(input, &token, name, name_len, jsbuf);
        break;
    case TOK_NUMBER:
        return ab_json_parse_number(input, &token, name, name_len, jsbuf);
        break;
    case TOK_ARRAY_B:
        ab_json_putback_token(input, &token);
        return ab_json_parse_array(input, name, name_len, jsbuf);
        break;
    case TOK_OBJECT_B:
        ab_json_putback_token(input, &token);
        return ab_json_parse_object(input, name, name_len, jsbuf);
        break;
    default:
        ab_json_putback_token(input, &token);
        return ab_return(ab_ret_json_invalid_syntax);
        break;
    }
}

static ab_ret_t ab_json_parse_object(ab_input_t *input, char *name, int name_len, ab_jsbuf_t *jsbuf)
{
    ab_token_t token = {0};
    ui8_t *begin = NULL;
    int ret;

    ab_json_get_token(input, &token);
    if (token.type != TOK_OBJECT_B)
    {
        ab_json_putback_token(input, &token);
        return ab_return(ab_ret_json_invalid_syntax);
    }

    ab_json_get_token(input, &token);
    if (token.type == TOK_OBJECT_E)
    {
        //empty object
        return ab_jsbuf_put_item(jsbuf, ABJSION_OBJECT, name, name_len, NULL, 0);
    }
    ab_json_putback_token(input, &token);

    if ((ret=ab_jsbuf_put_item(jsbuf, ABJSION_OBJECT, name, name_len, NULL, 0)) != ab_ret_ok) return ret;
    begin = jsbuf->current - 1;
    
    do
    {
        int parse_ret = ab_ret_ok;
        int name_len = 0;
        char *sub_name = NULL;

        //name
        ab_json_get_token(input, &token);
        if (token.type != TOK_STRING)
        {
            ab_json_putback_token(input, &token);
            return ab_return(ab_ret_json_expect_string);
        }

        name_len = ab_json_token_to_string(token.begin, token.len, &sub_name);
        if (name_len < 0) 
        {
            ab_json_putback_token(input, &token);
            return -name_len;
        }
        //:
        ab_json_get_token(input, &token);
        if (token.type != TOK_COLON)
        {
            ab_free(sub_name);
            ab_json_putback_token(input, &token);
            return ab_return(ab_ret_json_expect_colon);
        }
        //value
        parse_ret = ab_json_parse_value(input, sub_name, name_len, jsbuf);
        ab_free(sub_name);
        if (parse_ret != ab_ret_ok)
        {
            return parse_ret;
        }

        //check camma: ,
        ab_json_get_token(input, &token);
    } while(token.type == TOK_COMMA);

    if (token.type != TOK_OBJECT_E) 
    {
        ab_json_putback_token(input, &token);
        return ab_return(ab_ret_json_expect_brackets_o);
    }

    //insert object head
    return ab_jsbuf_update_len(jsbuf, begin, jsbuf->current-begin-1);
}

static ab_ret_t ab_json_parse_array(ab_input_t *input, char *name, int name_len, ab_jsbuf_t *jsbuf)
{
    char idname[32] = {0};
    unsigned long id = 0;
    size_t idnamelen = 0;

    ab_token_t token = {0};
    ui8_t *begin = NULL;
    int ret = 0;

    ab_json_get_token(input, &token);
    if (token.type != TOK_ARRAY_B)
    {
        ab_json_putback_token(input, &token);
        return ab_return(ab_ret_json_invalid_syntax);
    }

    ab_json_get_token(input, &token);
    if (token.type == TOK_ARRAY_E)
    {
        //empty array
        return ab_jsbuf_put_item(jsbuf, ABJSION_ARRAY, name, name_len, NULL, 0);    
    }
    ab_json_putback_token(input, &token);

    if ((ret=ab_jsbuf_put_item(jsbuf, ABJSION_ARRAY, name, name_len, NULL, 0)) != ab_ret_ok) return ret;
    begin = jsbuf->current - 1;
    
    do
    {
        //id name
        idnamelen = sprintf(idname, "%lu", id++);
        if ((ret = ab_json_parse_value(input, idname, idnamelen, jsbuf)) != ab_ret_ok) return ret;
        
        //check camma: ,
        ab_json_get_token(input, &token);
    } while(token.type == TOK_COMMA);

    if (token.type != TOK_ARRAY_E) 
    {
        ab_json_putback_token(input, &token);
        return ab_return(ab_ret_json_expect_brackets_a);
    }

    //insert array head
    return ab_jsbuf_update_len(jsbuf, begin, jsbuf->current-begin-1);
}

static ab_ret_t ab_json_parse_number(ab_input_t *input, ab_token_t *token, char *name, int name_len, ab_jsbuf_t *jsbuf)
{
    char *end = NULL;
    ui64_t vlong = 0;
    double vdouble = 0.0;
    
    //check as unsigned long
    vlong = strtoull(token->begin, &end, 10);
    if ((end == token->begin + token->len) && ((vlong>>63)&0x01) == 0)
    {
        ui8_t buf[32];
        ui32_t len = ab_jsbuf_encode_varint64(buf, vlong);
        return ab_jsbuf_put_item(jsbuf, ABJSION_ULONG, name, name_len, buf, len);
    }

    //check as double
    vdouble = strtod(token->begin, &end);
    if (end != token->begin + token->len)
    {
        ab_json_putback_token(input, token);
        return ab_return(ab_ret_json_invalid_number);
    }
    return ab_jsbuf_put_item(jsbuf, ABJSION_DOUBLE, name, name_len, (ui8_t*)&vdouble, sizeof(vdouble));
}

int ab_json_token_to_string(const char *begin, int len, char **str)
{
    char *dst = NULL;
    char *ret = NULL;
    const char *src = begin;
    
    ret = dst = (char *)ab_malloc(len+1);
    if (dst == NULL)
    {
        return (-ab_ret_outofmemory);
    }

    while (src < begin + len)
    {
        if (*src == '\\')
        {
            unsigned char length = 2;
            switch (*(src+1))
            {
            case 'b': *dst++ = '\b';   break;
            case 'f': *dst++ = '\f';   break;
            case 'n': *dst++ = '\n';   break;
            case 'r': *dst++ = '\r';   break;
            case 't': *dst++ = '\t';   break;
            case '\"':
            case '\\':
            case '/': *dst++ = *(src+1);  break;
            case 'u':
                length = utf16_literal_to_utf8(src, src+len, (unsigned char **)&dst);
                if (length == 0)
                {
                    ab_free(ret);
                    return (-ab_ret_json_invalid_unicode);
                }
                break;
            default:
                ab_free(ret);
                return (-ab_ret_json_invalid_escape);
            }
            src += length;
        }
        else
        {
            *dst++ = *src++;
        }
    }
    *dst = '\0';

    *str = ret;
    return (int)(dst-ret);
}

static ab_ret_t ab_json_parse_string(ab_input_t *input, ab_token_t *token, char *name, int name_len, ab_jsbuf_t *jsbuf)
{
    int ret = ab_ret_ok;
    char *strvalue = NULL;
    int len = ab_json_token_to_string(token->begin, token->len, &strvalue);
    if (len < 0)
    {
        ab_json_putback_token(input, token);
        return -len;
    }

    ret = ab_jsbuf_put_item(jsbuf, ABJSION_STRING, name, name_len, strvalue, len);
    ab_free(strvalue);
    return ret;
}


typedef struct
{
    unsigned char *buffer;
    size_t length;
    size_t offset;
    size_t depth; /* current nesting depth (for formatted printing) */
    int noalloc;
    int format; /* is this print a formatted print */
} printbuffer;

#define ab_min(a, b) (((a) < (b)) ? (a) : (b))
static unsigned char* ensure(printbuffer * const p, size_t needed);
static void update_offset(printbuffer * const buffer);
static ab_ret_t print_string_ptr(const ui8_t* const input, ui32_t input_len, printbuffer *output_buffer);
static ab_ret_t ab_json_print_string(const js_item_t *item, printbuffer * output_buffer);
static ab_ret_t ab_json_print_double(const js_item_t *item, printbuffer * output_buffer);
static ab_ret_t ab_json_print_ulong(const js_item_t *item, printbuffer * output_buffer);
static ab_ret_t ab_json_print_array(const js_item_t *item, printbuffer * output_buffer);
static ab_ret_t ab_json_print_object(const js_item_t *item, printbuffer * output_buffer);
static ab_ret_t ab_json_print_value(const js_item_t *item, printbuffer *output_buffer);
static char *ab_json_print_func(ui8_t *jsbin, int is_format);

void ab_json_snprint(ui8_t *jsbin, int is_format, char *out_buf, int size)
{
    char *jstr = ab_json_print_func(jsbin, is_format);
    if (jstr == NULL){
        return;
    } 

    strncpy(out_buf, jstr, size-1);
    out_buf[size-1] = '\0';

    ab_free(jstr);
}

void ab_json_print(ui8_t *jsbin, int is_format)
{
    char *jstr = ab_json_print_func(jsbin, is_format);
    if (jstr == NULL){
        return;
    }
    printf("%s\n", jstr);
    ab_free(jstr);
}

static char *ab_json_print_func(ui8_t *jsbin, int is_format)
{
    js_item_t item = {0};
    ab_jsbuf_t jsbuf = {0};
    static const size_t default_buffer_size = 256;
    printbuffer buffer[1];
    unsigned char *printed = NULL;
    int ret;

    memset(buffer, 0, sizeof(buffer));

    jsbuf.current = jsbuf.buf = ((ab_jsbin_t*)jsbin)->buf;
    jsbuf.size = ((ab_jsbin_t*)jsbin)->jsbin_len;
    if (!ab_jsbuf_get_item(&jsbuf, &item)) return NULL;

    /* create buffer */
    buffer->buffer = (unsigned char*)ab_malloc(default_buffer_size);
    buffer->length = default_buffer_size;
    buffer->format = is_format;
    if (buffer->buffer == NULL)
    {
        return NULL;
    }

    /* print the value */
    if ((ret = ab_json_print_value(&item, buffer)) != ab_ret_ok)
    {
        printf("err:%s\b", buffer->buffer);
        goto fail;
    }
    update_offset(buffer);
    printed = (unsigned char*)ab_malloc(buffer->offset + 1);
    if (printed == NULL)
    {
        goto fail;
    }
    memcpy(printed, buffer->buffer, ab_min(buffer->length, buffer->offset + 1));
    printed[buffer->offset] = '\0'; /* just to be sure */
    ab_free(buffer->buffer);
    return (char *)printed;

fail:
    if (buffer->buffer != NULL)
    {
        ab_free(buffer->buffer);
    }

    if (printed != NULL)
    {
        ab_free(printed);
    }

    return NULL;
}

static ab_ret_t ab_json_print_value(const js_item_t *item, printbuffer *output_buffer)
{
    unsigned char *output = NULL;
    int ret;
    
    switch (item->type)
    {
        case ABJSION_NULL:
        case ABJSION_UNKNOW:
            output = ensure(output_buffer, 5);
            if (output == NULL)
            {
                return ab_ret_outofmemory;
            }
            strcpy((char*)output, "null");
            return ab_ret_ok;

        case ABJSION_FALSE:
            output = ensure(output_buffer, 6);
            if (output == NULL)
            {
                return ab_ret_outofmemory;
            }
            strcpy((char*)output, "false");
            return ab_ret_ok;

        case ABJSION_TRUE:
            output = ensure(output_buffer, 5);
            if (output == NULL)
            {
                return ab_ret_outofmemory;
            }
            strcpy((char*)output, "true");
            return ab_ret_ok;

        case ABJSION_DOUBLE:
            return ab_json_print_double(item, output_buffer);

        case ABJSION_ULONG:
            return ab_json_print_ulong(item, output_buffer);

        case ABJSION_STRING:
            return ab_json_print_string(item, output_buffer);

        case ABJSION_ARRAY:
            return ab_json_print_array(item, output_buffer);

        case ABJSION_OBJECT:
            return ab_json_print_object(item, output_buffer);
            
        default:
            return ab_return(ab_ret_json_invalid_type);
    }
}

static ab_ret_t ab_json_print_object(const js_item_t *item, printbuffer * output_buffer)
{
    ab_jsbuf_t jsbuf = {0};
    unsigned char *output_pointer = NULL;
    size_t length = 0;
    int ret;

    if (output_buffer == NULL)
    {
        return ab_ret_invalid_ptr;
    }

    /* Compose the output: */
    length = (size_t) (output_buffer->format ? 2 : 1); /* fmt: {\n */
    output_pointer = ensure(output_buffer, length + 1);
    if (output_pointer == NULL)
    {
        return ab_ret_outofmemory;
    }

    *output_pointer++ = '{';
    output_buffer->depth++;
    if (output_buffer->format)
    {
        *output_pointer++ = '\n';
    }
    output_buffer->offset += length;

    jsbuf.current = jsbuf.buf = item->value;
    jsbuf.size = item->value_len;
    while (jsbuf.current < jsbuf.buf + jsbuf.size)
    {
        js_item_t sub_item = {0};
        if (!ab_jsbuf_get_item(&jsbuf, &sub_item)) return ab_ret_jsonbuf_no_left_content;

        if (output_buffer->format)
        {
            size_t i;
            output_pointer = ensure(output_buffer, output_buffer->depth);
            if (output_pointer == NULL)
            {
                return ab_ret_outofmemory;
            }
            for (i = 0; i < output_buffer->depth; i++)
            {
                *output_pointer++ = '\t';
            }
            output_buffer->offset += output_buffer->depth;
        }

        /* print key */
        if ((ret=print_string_ptr(sub_item.name, sub_item.name_len, output_buffer)) != ab_ret_ok)
        {
            return ret;
        }
        update_offset(output_buffer);

        length = (size_t) (output_buffer->format ? 2 : 1);
        output_pointer = ensure(output_buffer, length);
        if (output_pointer == NULL)
        {
            return ab_ret_outofmemory;
        }
        *output_pointer++ = ':';
        if (output_buffer->format)
        {
            *output_pointer++ = '\t';
        }
        output_buffer->offset += length;

        /* print value */
        if ((ret=ab_json_print_value(&sub_item, output_buffer)) != ab_ret_ok)
        {
            return ret;
        }
        update_offset(output_buffer);

        /* print comma if not last */
        length = ((size_t)(output_buffer->format ? 1 : 0) + (size_t)(jsbuf.current < jsbuf.buf + jsbuf.size ? 1 : 0));
        output_pointer = ensure(output_buffer, length + 1);
        if (output_pointer == NULL)
        {
            return ab_ret_outofmemory;
        }
        if (jsbuf.current < jsbuf.buf + jsbuf.size)
        {
            *output_pointer++ = ',';
        }

        if (output_buffer->format)
        {
            *output_pointer++ = '\n';
        }
        *output_pointer = '\0';
        output_buffer->offset += length;
    }

    output_pointer = ensure(output_buffer, output_buffer->format ? (output_buffer->depth + 1) : 2);
    if (output_pointer == NULL)
    {
        return ab_ret_outofmemory;
    }
    if (output_buffer->format)
    {
        size_t i;
        for (i = 0; i < (output_buffer->depth - 1); i++)
        {
            *output_pointer++ = '\t';
        }
    }
    *output_pointer++ = '}';
    *output_pointer = '\0';
    output_buffer->depth--;

    return ab_ret_ok;
}

static ab_ret_t ab_json_print_array(const js_item_t *item, printbuffer * output_buffer)
{
    ab_jsbuf_t jsbuf = {0};
    unsigned char *output_pointer = NULL;
    size_t length = 0;
    int ret;

    if (output_buffer == NULL)
    {
        return ab_ret_invalid_ptr;
    }

    /* Compose the output array. */
    /* opening square bracket */
    output_pointer = ensure(output_buffer, 1);
    if (output_pointer == NULL)
    {
        return ab_ret_outofmemory;
    }

    *output_pointer = '[';
    output_buffer->offset++;
    output_buffer->depth++;

    jsbuf.current = jsbuf.buf = item->value;
    jsbuf.size = item->value_len;
    while (jsbuf.current < jsbuf.buf + jsbuf.size)
    {
        js_item_t sub_item = {0};
        if (!ab_jsbuf_get_item(&jsbuf, &sub_item)) return ab_ret_jsonbuf_no_left_content;

        if ((ret = ab_json_print_value(&sub_item, output_buffer)) != ab_ret_ok)
        {
            return ret;
        }
        update_offset(output_buffer);
        if (jsbuf.current < jsbuf.buf + jsbuf.size)
        {
            length = (size_t) (output_buffer->format ? 2 : 1);
            output_pointer = ensure(output_buffer, length + 1);
            if (output_pointer == NULL)
            {
                return ab_ret_outofmemory;
            }
            *output_pointer++ = ',';
            if(output_buffer->format)
            {
                *output_pointer++ = ' ';
            }
            *output_pointer = '\0';
            output_buffer->offset += length;
        }
    }

    output_pointer = ensure(output_buffer, 2);
    if (output_pointer == NULL)
    {
        return ab_ret_outofmemory;
    }
    *output_pointer++ = ']';
    *output_pointer = '\0';
    output_buffer->depth--;

    return ab_ret_ok;
}

static ab_ret_t print_string_ptr(const ui8_t* const input, ui32_t input_len, printbuffer *output_buffer)
{
    const ui8_t *input_pointer = NULL;
    const ui8_t *input_end = input+input_len;
    ui8_t *output = NULL;
    ui8_t *output_pointer = NULL;
    size_t output_length = 0;
    /* numbers of additional characters needed for escaping */
    size_t escape_characters = 0;

    if (output_buffer == NULL)
    {
        return ab_ret_invalid_ptr;
    }

    /* empty string */
    if (input == NULL)
    {
        output = ensure(output_buffer, sizeof("\"\""));
        if (output == NULL)
        {
            return ab_ret_outofmemory;
        }
        strcpy((char*)output, "\"\"");

        return ab_ret_ok;
    }

    /* set "flag" to 1 if something needs to be escaped */
    for (input_pointer = input; input_pointer < input_end && *input_pointer; input_pointer++)
    {
        switch (*input_pointer)
        {
            case '\"':
            case '\\':
            case '\b':
            case '\f':
            case '\n':
            case '\r':
            case '\t':
                /* one character escape sequence */
                escape_characters++;
                break;
            default:
                if (*input_pointer < 32)
                {
                    /* UTF-16 escape sequence uXXXX */
                    escape_characters += 5;
                }
                break;
        }
    }
    output_length = (size_t)(input_pointer - input) + escape_characters;

    output = ensure(output_buffer, output_length + sizeof("\"\""));
    if (output == NULL)
    {
        return ab_ret_outofmemory;
    }

    /* no characters have to be escaped */
    if (escape_characters == 0)
    {
        output[0] = '\"';
        memcpy(output + 1, input, output_length);
        output[output_length + 1] = '\"';
        output[output_length + 2] = '\0';

        return ab_ret_ok;
    }

    output[0] = '\"';
    output_pointer = output + 1;
    /* copy the string */
    for (input_pointer = input; input_pointer < input_end && *input_pointer != '\0'; (void)input_pointer++, output_pointer++)
    {
        if ((*input_pointer > 31) && (*input_pointer != '\"') && (*input_pointer != '\\'))
        {
            /* normal character, copy */
            *output_pointer = *input_pointer;
        }
        else
        {
            /* character needs to be escaped */
            *output_pointer++ = '\\';
            switch (*input_pointer)
            {
                case '\\':
                    *output_pointer = '\\';
                    break;
                case '\"':
                    *output_pointer = '\"';
                    break;
                case '\b':
                    *output_pointer = 'b';
                    break;
                case '\f':
                    *output_pointer = 'f';
                    break;
                case '\n':
                    *output_pointer = 'n';
                    break;
                case '\r':
                    *output_pointer = 'r';
                    break;
                case '\t':
                    *output_pointer = 't';
                    break;
                default:
                    /* escape and print as unicode codepoint */
                    sprintf((char*)output_pointer, "u%04x", *input_pointer);
                    output_pointer += 4;
                    break;
            }
        }
    }
    output[output_length + 1] = '\"';
    output[output_length + 2] = '\0';

    return ab_ret_ok;
}


static ab_ret_t ab_json_print_string(const js_item_t *item, printbuffer * output_buffer)
{
    return print_string_ptr(item->value, item->value_len, output_buffer);
}

static ab_ret_t ab_json_print_double(const js_item_t *item, printbuffer * output_buffer)
{
    unsigned char *output_pointer = NULL;
    unsigned char number_buffer[26] = {0}; 
    int length = 0;
    double vdouble = *(double*)item->value;

    if (output_buffer == NULL)
    {
        return ab_ret_invalid_ptr;
    }

    length = sprintf((char*)number_buffer, "%1.15g", vdouble);
    if ((length < 0) || (length > (int)(sizeof(number_buffer) - 1)))
    {
        return ab_return(ab_ret_json_invalid_number);
    }

    output_pointer = ensure(output_buffer, (size_t)length + sizeof(""));
    if (output_pointer == NULL)
    {
        return ab_ret_outofmemory;
    }
    strcpy(output_pointer, number_buffer);
    output_buffer->offset += (size_t)length;

    return ab_ret_ok;
}

static ab_ret_t ab_json_print_ulong(const js_item_t *item, printbuffer * output_buffer)
{
    unsigned char *output_pointer = NULL;
    unsigned char number_buffer[26] = {0}; 
    int length = 0;
    ui64_t ulvalue = 0;
    
    if (output_buffer == NULL)
    {
        return ab_ret_invalid_ptr;
    }

    ab_jsbuf_get_varint64(item->value, &ulvalue);
    length = sprintf((char*)number_buffer, "%llu", ulvalue);
    if ((length < 0) || (length > (int)(sizeof(number_buffer) - 1)))
    {
        return ab_return(ab_ret_json_invalid_number);
    }

    output_pointer = ensure(output_buffer, (size_t)length + sizeof(""));
    if (output_pointer == NULL)
    {
        return ab_ret_outofmemory;
    }
    strcpy(output_pointer, number_buffer);
    output_buffer->offset += (size_t)length;

    return ab_ret_ok;
}

static unsigned char* ensure(printbuffer * const p, size_t needed)
{
    unsigned char *newbuffer = NULL;
    size_t newsize = 0;

    if ((p == NULL) || (p->buffer == NULL))
    {
        return NULL;
    }

    if ((p->length > 0) && (p->offset >= p->length))
    {
        return NULL;
    }

    if (needed > INT_MAX)
    {
        return NULL;
    }

    needed += p->offset + 1;
    if (needed <= p->length)
    {
        return p->buffer + p->offset;
    }

    if (p->noalloc) {
        return NULL;
    }

    /* calculate new buffer size */
    if (needed > (INT_MAX / 2))
    {
        /* overflow of int, use INT_MAX if possible */
        if (needed <= INT_MAX)
        {
            newsize = INT_MAX;
        }
        else
        {
            return NULL;
        }
    }
    else
    {
        newsize = needed * 2;
    }

    newbuffer = (unsigned char*)ab_malloc(newsize);
    if (!newbuffer)
    {
        ab_free(p->buffer);
        p->length = 0;
        p->buffer = NULL;

        return NULL;
    }

    memcpy(newbuffer, p->buffer, p->offset + 1);
    ab_free(p->buffer);
    
    p->length = newsize;
    p->buffer = newbuffer;

    return newbuffer + p->offset;
}

static void update_offset(printbuffer * const buffer)
{
    const unsigned char *buffer_pointer = NULL;
    if ((buffer == NULL) || (buffer->buffer == NULL))
    {
        return;
    }
    buffer_pointer = buffer->buffer + buffer->offset;

    buffer->offset += strlen((const char*)buffer_pointer);
}


// token functions 

static void ab_json_skip_whitespace(ab_input_t *input)
{
    if (input == NULL || input->buffer == NULL || input->current == NULL)
    {
        return;
    }

    while (input->left > 0 && input->current[0] <= 32)
    {
        input->current++;
        input->left--;
    }
}

void ab_json_putback_token(ab_input_t *input, ab_token_t *token)
{
    if (token->type != TOK_END && 
        token->type != TOK_INVALID &&
        input->buffer + token->len <= input->current)
    {
        input->current -= token->len;
        input->left += token->len;
        if (token->type == TOK_STRING && input->current > input->buffer)
        {
            input->current -=2;  //putback ""
            input->left +=2;
        }
    }
}

void ab_json_get_token(ab_input_t *input, ab_token_t *token)
{
    ab_json_skip_whitespace(input);

    //end
    if (input->left <= 0 || input->current[0] == '\0')
    {
        token->begin = input->current;
        token->len = 0;
        token->type = TOK_END;
        return;
    }
    
    //string
    if (input->current[0] == '\"')
    {
        input->current++;
        input->left--;

        token->begin = input->current;
        token->len   = 0;

        //search end of string
        while(input->left >= 1)
        {
            if (*input->current == '\\')
            {
                if (input->left < 2)
                {
                    token->type = TOK_INVALID;
                    return;
                }
                input->current += 2;
                input->left -= 2;
                token->len += 2;
            }
            else if (*input->current == '\"')
            {
                input->current += 1;
                input->left -= 1;

                token->type = TOK_STRING;
                return;
            }
            else
            {
                input->current += 1;
                input->left -= 1;
                token->len += 1;
            }
        }
        
        token->type = TOK_INVALID;
        return;
    }

    //number
    if ((input->current[0] >= '0' && input->current[0] <= '9') || input->current[0] == '-')
    {
        token->begin = input->current;
        token->type  = TOK_NUMBER;
        token->len   = 0;

        while(input->left >= 1)
        {
            switch (*input->current)
            {
            case '0':  case '1':  case '2':  case '3':  case '4':  
            case '5':  case '6':  case '7':  case '8':  case '9':  
            case '+':  case '-':  case 'e':  case 'E':  case '.':
                input->current++;
                input->left--;
                token->len++;
                break;
            default:
                return;
            }
        }
        return;
    }

    //{}[],:
    if (strchr(":,{}[]", input->current[0]))
    {
        switch (input->current[0])
        {
        case '{': token->type = TOK_OBJECT_B; break;
        case '}': token->type = TOK_OBJECT_E; break;
        case '[': token->type = TOK_ARRAY_B;  break;
        case ']': token->type = TOK_ARRAY_E;  break;
        case ',': token->type = TOK_COMMA;    break;
        case ':': token->type = TOK_COLON;    break;
        }
        token->begin = input->current;
        token->len = 1;

        input->current ++;
        input->left --;
        return;
    }
    //null
    if (input->left >= 4 && 
        input->current[0] == 'n' && 
        input->current[1] == 'u' && 
        input->current[2] == 'l' && 
        input->current[3] == 'l' )
    {
        token->begin = input->current;
        token->len = 4;
        token->type = TOK_NULL;

        input->current += 4;
        input->left -= 4;
        return;
    }
    //true
    if (input->left >= 4 && 
        input->current[0] == 't' && 
        input->current[1] == 'r' && 
        input->current[2] == 'u' && 
        input->current[3] == 'e' )
    {
        token->begin = input->current;
        token->len = 4;
        token->type = TOK_TRUE;

        input->current += 4;
        input->left -= 4;
        return;
    }
    //false
    if (input->left >= 5 && 
        input->current[0] == 'f' && 
        input->current[1] == 'a' && 
        input->current[2] == 'l' && 
        input->current[3] == 's' && 
        input->current[4] == 'e' )
    {
        token->begin = input->current;
        token->len = 5;
        token->type = TOK_FALSE;

        input->current += 5;
        input->left -= 5;
        return;
    }

    //invalid
    token->begin = input->current;
    token->len = 0;
    token->type = TOK_INVALID;
    return;
}

/* parse 4 digit hexadecimal number */
static unsigned parse_hex4(const unsigned char * const input)
{
    unsigned int h = 0;
    size_t i = 0;

    for (i = 0; i < 4; i++)
    {
        /* parse digit */
        if ((input[i] >= '0') && (input[i] <= '9'))
        {
            h += (unsigned int) input[i] - '0';
        }
        else if ((input[i] >= 'A') && (input[i] <= 'F'))
        {
            h += (unsigned int) 10 + input[i] - 'A';
        }
        else if ((input[i] >= 'a') && (input[i] <= 'f'))
        {
            h += (unsigned int) 10 + input[i] - 'a';
        }
        else /* invalid */
        {
            return 0;
        }

        if (i < 3)
        {
            /* shift left to make place for the next nibble */
            h = h << 4;
        }
    }

    return h;
}

/* converts a UTF-16 literal to UTF-8
 * A literal can be one or two sequences of the form \uXXXX */
static unsigned char utf16_literal_to_utf8(const unsigned char * const input_pointer, const unsigned char * const input_end, unsigned char **output_pointer)
{
    long unsigned int codepoint = 0;
    unsigned int first_code = 0;
    const unsigned char *first_sequence = input_pointer;
    unsigned char utf8_length = 0;
    unsigned char utf8_position = 0;
    unsigned char sequence_length = 0;
    unsigned char first_byte_mark = 0;

    if ((input_end - first_sequence) < 6)
    {
        /* input ends unexpectedly */
        goto fail;
    }

    /* get the first utf16 sequence */
    first_code = parse_hex4(first_sequence + 2);

    /* check that the code is valid */
    if (((first_code >= 0xDC00) && (first_code <= 0xDFFF)))
    {
        goto fail;
    }

    /* UTF16 surrogate pair */
    if ((first_code >= 0xD800) && (first_code <= 0xDBFF))
    {
        const unsigned char *second_sequence = first_sequence + 6;
        unsigned int second_code = 0;
        sequence_length = 12; /* \uXXXX\uXXXX */

        if ((input_end - second_sequence) < 6)
        {
            /* input ends unexpectedly */
            goto fail;
        }

        if ((second_sequence[0] != '\\') || (second_sequence[1] != 'u'))
        {
            /* missing second half of the surrogate pair */
            goto fail;
        }

        /* get the second utf16 sequence */
        second_code = parse_hex4(second_sequence + 2);
        /* check that the code is valid */
        if ((second_code < 0xDC00) || (second_code > 0xDFFF))
        {
            /* invalid second half of the surrogate pair */
            goto fail;
        }


        /* calculate the unicode codepoint from the surrogate pair */
        codepoint = 0x10000 + (((first_code & 0x3FF) << 10) | (second_code & 0x3FF));
    }
    else
    {
        sequence_length = 6; /* \uXXXX */
        codepoint = first_code;
    }

    /* encode as UTF-8
     * takes at maximum 4 bytes to encode:
     * 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx */
    if (codepoint < 0x80)
    {
        /* normal ascii, encoding 0xxxxxxx */
        utf8_length = 1;
    }
    else if (codepoint < 0x800)
    {
        /* two bytes, encoding 110xxxxx 10xxxxxx */
        utf8_length = 2;
        first_byte_mark = 0xC0; /* 11000000 */
    }
    else if (codepoint < 0x10000)
    {
        /* three bytes, encoding 1110xxxx 10xxxxxx 10xxxxxx */
        utf8_length = 3;
        first_byte_mark = 0xE0; /* 11100000 */
    }
    else if (codepoint <= 0x10FFFF)
    {
        /* four bytes, encoding 1110xxxx 10xxxxxx 10xxxxxx 10xxxxxx */
        utf8_length = 4;
        first_byte_mark = 0xF0; /* 11110000 */
    }
    else
    {
        /* invalid unicode codepoint */
        goto fail;
    }

    /* encode as utf8 */
    for (utf8_position = (unsigned char)(utf8_length - 1); utf8_position > 0; utf8_position--)
    {
        /* 10xxxxxx */
        (*output_pointer)[utf8_position] = (unsigned char)((codepoint | 0x80) & 0xBF);
        codepoint >>= 6;
    }
    /* encode first byte */
    if (utf8_length > 1)
    {
        (*output_pointer)[0] = (unsigned char)((codepoint | first_byte_mark) & 0xFF);
    }
    else
    {
        (*output_pointer)[0] = (unsigned char)(codepoint & 0x7F);
    }

    *output_pointer += utf8_length;

    return sequence_length;

fail:
    return 0;
}


///////////////////////////////////////////////////////////////////////////////////////////
// js_buf_t encode/decode funtions
///////////////////////////////////////////////////////////////////////////////////////////

static ui32_t ab_jsbuf_encode_varint32(ui8_t* dst, ui32_t v) 
{
    ui8_t* ptr = dst;
    static const int B = 128;
    if (v < (1 << 7)) {
      *(ptr++) = v;
    } else if (v < (1 << 14)) {
      *(ptr++) = v | B;
      *(ptr++) = v >> 7;
    } else if (v < (1 << 21)) {
      *(ptr++) = v | B;
      *(ptr++) = (v >> 7) | B;
      *(ptr++) = v >> 14;
    } else if (v < (1 << 28)) {
      *(ptr++) = v | B;
      *(ptr++) = (v >> 7) | B;
      *(ptr++) = (v >> 14) | B;
      *(ptr++) = v >> 21;
    } else {
      *(ptr++) = v | B;
      *(ptr++) = (v >> 7) | B;
      *(ptr++) = (v >> 14) | B;
      *(ptr++) = (v >> 21) | B;
      *(ptr++) = v >> 28;
    }
    return (ui32_t)(ptr - dst);
}

ui32_t ab_jsbuf_encode_varint64(ui8_t* dst, ui64_t v) 
{
    static const int B = 128;
    ui8_t* ptr = dst;
    while (v >= B) {
        *(ptr++) = v | B;
        v >>= 7;
    }
    *(ptr++) = (ui8_t)v;
    return (ui32_t)(ptr - dst);
}

ui8_t* ab_jsbuf_get_varint32(ui8_t* p, ui32_t* value) 
{
    ui32_t shift = 0;
    ui32_t result = 0;

    if ((*p & 128) == 0) 
    {
      *value = *p;
      return p + 1;
    }

    for (shift = 0; shift <= 28; shift += 7) 
    {
        ui32_t byte = *p++;
        if (byte & 128) {
            result |= ((byte & 127) << shift);
        } else {
            result |= (byte << shift);
            *value = result;
            return p;
        }
    }
    return NULL;
}

ui8_t* ab_jsbuf_get_varint64(ui8_t* p, ui64_t* value) 
{
    ui32_t shift = 0;
    ui64_t result = 0;
    
    for (shift = 0; shift <= 63; shift += 7) 
    {
        ui64_t byte = *p++;
        if (byte & 128) {
            // More bytes are present
            result |= ((byte & 127) << shift);
        } else {
            result |= (byte << shift);
            *value = result;
            return p;
        }
    }
    return NULL;
}

void ab_jsbuf_test_varint()
{
    char buf[12];
    ui64_t value64 = 0;
    ui32_t value = 0;
    ui32_t len = ab_jsbuf_encode_varint32(buf, 4294967295);
    ab_jsbuf_get_varint32(buf, &value);
    printf("len=%u value=%u\n", len, value);

    len = ab_jsbuf_encode_varint32(buf, 128);
    ab_jsbuf_get_varint32(buf, &value);
    printf("len=%u value=%u\n", len, value);

    len = ab_jsbuf_encode_varint32(buf, 127);
    ab_jsbuf_get_varint32(buf, &value);
    printf("len=%u value=%u\n", len, value);


    len = ab_jsbuf_encode_varint64(buf, 127);
    ab_jsbuf_get_varint64(buf, &value64);
    printf("len=%u value=%llu\n", len, value64);
}

static int ab_jsbuf_append(ab_jsbuf_t *jsbuf, ui8_t *append, int append_len)
{
    if (jsbuf->current + append_len > jsbuf->buf + jsbuf->size) return ab_ret_jsonbuf_no_left_space;

    memcpy(jsbuf->current, append, append_len);
    jsbuf->current += append_len;
    return ab_ret_ok;
}

static int ab_jsbuf_put_varint32(ab_jsbuf_t* jsbuf, ui32_t v) 
{
    ui8_t buf[5];
    ui32_t len = ab_jsbuf_encode_varint32(buf, v);
    return ab_jsbuf_append(jsbuf, buf, len);
}

static int ab_jsbuf_put_fixint8(ab_jsbuf_t* jsbuf, ui8_t v) 
{
    ui8_t buf[1];
    buf[0] = v;
    return ab_jsbuf_append(jsbuf, buf, 1);
}

//type varint(name_len) name varint(value_len) value
static int ab_jsbuf_put_head(ab_jsbuf_t* jsbuf, ui8_t type, ui8_t *name, ui32_t name_len)
{
    int ret = ab_ret_ok;
    if ((ret = ab_jsbuf_put_fixint8(jsbuf, type)) != ab_ret_ok) return ret;
    if ((ret = ab_jsbuf_put_varint32(jsbuf, name_len)) != ab_ret_ok) return ret;
    if (name_len != 0) return ab_jsbuf_append(jsbuf, name, name_len);
    return ab_ret_ok;
}

int ab_jsbuf_put_item(ab_jsbuf_t* jsbuf, ui8_t type, ui8_t *name, ui32_t name_len, ui8_t *value, ui32_t value_len)
{
    int ret = ab_ret_ok;
    if ((ret = ab_jsbuf_put_head(jsbuf, type, name, name_len)) != ab_ret_ok) return ret;

    if ((ret = ab_jsbuf_put_varint32(jsbuf, value_len)) != ab_ret_ok) return ret;
    if (value_len != 0) return ab_jsbuf_append(jsbuf, value, value_len);
    return ab_ret_ok;
}


static int ab_jsbuf_update_len(ab_jsbuf_t* jsbuf, ui8_t *pos, int diff_len)
{
    ui32_t move_len = 0;
    ui32_t old_len = 0;
    ui8_t *value_pos = ab_jsbuf_get_varint32(pos, &old_len);
    ui32_t old_bytes = value_pos - pos;
    
    ui8_t buf[5];
    ui32_t new_len = old_len + diff_len;
    ui32_t new_bytes = ab_jsbuf_encode_varint32(buf, new_len);
    int bytes_diff = new_bytes - old_bytes;

    if (new_bytes == old_bytes)
    {
        memcpy(pos, buf, new_bytes);
        return ab_ret_ok;
    }

    if (jsbuf->current + bytes_diff > jsbuf->buf + jsbuf->size) 
        return ab_ret_jsonbuf_no_left_space;

    move_len = jsbuf->current - pos - old_bytes;
    memmove(pos + new_bytes, pos + old_bytes, move_len);
    memcpy(pos, buf, new_bytes);

    jsbuf->current += bytes_diff;
    return ab_ret_ok;
}

int ab_jsbuf_update_len_func(ui8_t* buf, ui32_t *len, ui32_t buf_len, ui8_t *pos, int diff_len)
{
    int ret = 0;
    ab_jsbuf_t jsbuf = {0};

    jsbuf.buf = buf;
    jsbuf.current = buf+(*len);
    jsbuf.size = buf_len;

    ret = ab_jsbuf_update_len(&jsbuf, pos, diff_len);
    if (ret == ab_ret_ok)
    {
        *len = jsbuf.current - jsbuf.buf;
    }

    return ret;
}

ui8_t *ab_jsbuf_get_item(ab_jsbuf_t* jsbuf, js_item_t *item)
{
    ui8_t *end = jsbuf->buf + jsbuf->size;

    //save org buf
    item->orgbuf = jsbuf->buf;

    //type
    item->type = *jsbuf->current++;
    if (jsbuf->current >= end) return NULL;

    //name-len
    jsbuf->current = ab_jsbuf_get_varint32(jsbuf->current, &item->name_len);
    if (jsbuf->current >= end) return NULL;
    //name
    item->name = jsbuf->current;
    jsbuf->current += item->name_len;
    if (jsbuf->current >= end) return NULL;

    //value-len
    jsbuf->current =  ab_jsbuf_get_varint32(jsbuf->current, &item->value_len);
    if (jsbuf->current > end) return NULL;
    //value
    item->value = jsbuf->current;
    jsbuf->current += item->value_len;
    if (jsbuf->current > end) return NULL;
    
    return jsbuf->current;
}

ui8_t* ab_jsbuf_get_child(ab_jsbuf_t* jsbuf)
{
    js_item_t item = {0};
    ui8_t *next = ab_jsbuf_get_item(jsbuf, &item);
    if (!next) return NULL;
    if (item.value_len == 0) return NULL;
    if (item.type != ABJSION_ARRAY && item.type != ABJSION_OBJECT) return NULL;

    return item.value;
}

ui8_t* ab_jsbuf_get_next(ab_jsbuf_t* jsbuf)
{
    js_item_t item = {0};
    ui8_t *next = ab_jsbuf_get_item(jsbuf, &item);
    if (!next) return NULL;
    if (next >= jsbuf->buf + jsbuf->size) return NULL;
    
    return next;
}

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>






/* JSON-PATH Simple BNF define:
    path ::= head '.' tail | head
    head ::= '$' | '@'
    tail ::= (recursive_descent | child_operator | filter_operator | subscript_operator | array_slice_operator) |  tail

    recursive_descent ::= '..' identifier
    child_operator ::= '.' identifier
    filter_operator ::= '[?(' script_expression ')]'
    subscript_operator ::= '[' index ']'
    array_slice_operator ::= '[' slice_parameters ']'

    index ::= number | wildcard | union | script_expression
    number ::= digit+
    wildcard ::= '*'
    union ::= index(',' index)*

    slice_parameters ::= start ':' end ':' step | start ':' end | start ':'
    start ::= number | script_expression | ε
    end   ::= number | script_expression | ε
    step  ::= number | script_expression | ε

    script_expression ::= '(' expression ')'
    expression ::= ... // 根据具体需求进行定义
*/

typedef enum {
    TOKEN_ROOT,        // $  the root object/element
    TOKEN_CURRENT,     // @  the current object/element
    TOKEN_CHILD,       // .  child operator
    TOKEN_SUB_START,   // [  
    TOKEN_SUB_END,     // ]  
    TOKEN_RECURSIVE,   // .. recursive descent. JSONPath borrows this syntax from E4X
    TOKEN_WILDCARD,    // *  wildcard. All objects/elements regardless their names
    TOKEN_UNION,       // [,]Union operator in XPath results in a combination of node sets. JSONPath allows alternate names or array indices as a set.
    TOKEN_SLICE,       // [start:end:step] array slice operator borrowed from ES4.
    TOKEN_FILTER,      // ?() applies a filter (script) expression.
    TOKEN_EXP_START,   // ()  script expression, using the underlying script engine.
    TOKEN_EXP_END,     // ()  script expression, using the underlying script engine.
    TOKEN_NUMBER,      // number
    TOKEN_STRING,      // string
    TOKEN_ID,          // identifier
    TOKEN_OPR_ADD,     // +
    TOKEN_OPR_SUB,     // -
    TOKEN_OPR_MUL,     // *
    TOKEN_OPR_DEV,     // /
    TOKEN_OPR_MOD,     // %
    TOKEN_OPR_EQ,      // ==
    TOKEN_OPR_NE,      // !=
    TOKEN_OPR_GT,      // >
    TOKEN_OPR_LT,      // <
    TOKEN_OPR_GE,      // >=
    TOKEN_OPR_LE,      // <=
    TOKEN_OPR_OR,      // ||
    TOKEN_OPR_AND,     // &&
    TOKEN_INVALID,
    TOKEN_END
} jspath_token_type_t;

typedef struct 
{
    jspath_token_type_t type;
    const char* lexeme;
    int len;
} jspath_token_t;

typedef struct 
{
    bool is_mkdir;
    const char *path;

    int token_cur;
    int token_num;
    jspath_token_t *tokens;

    ab_jsbuf_t jsbuf;
    ui32_t jsbuf_len;
    ab_list_t *set;
    ab_list_t *tmp;
    ab_hash_t *parent_map;
} jspath_parser_t;

enum{
    VTNUM,
    VTSTR,
};

typedef struct 
{
    int type;       //VTNUM | VTSTR
    int strlen;
    const char *str;
    double number;
} jspath_value_t;


#define ab_jspath_get_token(parser)  (parser)->token_cur++
#define ab_jspath_put_back(parser)   (parser)->token_cur--
#define cur_token(parser)            (parser)->tokens[(parser)->token_cur]

#define __is_true(v)    (((v)->type == VTNUM) ? ((int)(v)->number != 0) : ((v)->str && (v)->str[0]))

static int ab_jspath_get_all_token(jspath_parser_t *parser, const char* path);
static int ab_jspath_parse_tail(jspath_parser_t *parser);
static int ab_jspath_parse_recursive_descent(jspath_parser_t *parser) ;
static int ab_jspath_parse_child_operator(jspath_parser_t *parser);
static int ab_jspath_parse_filter_operator(jspath_parser_t *parser);
static int ab_jspath_parse_subscript_operator(jspath_parser_t *parser);
static int ab_jspath_parse_array_slice_operator(jspath_parser_t *parser);

static int ab_jspath_parse_expression(jspath_parser_t *parser, jspath_value_t *val);
static int ab_jspath_exp_logic_or(jspath_parser_t *parser, jspath_value_t *val);
static int ab_jspath_exp_logic_and(jspath_parser_t *parser, jspath_value_t *val);
static int ab_jspath_exp_equality(jspath_parser_t *parser, jspath_value_t *val);
static int ab_jspath_exp_relational(jspath_parser_t *parser, jspath_value_t *val);
static int ab_jspath_exp_additive(jspath_parser_t *parser, jspath_value_t *val);
static int ab_jspath_exp_multiplicative(jspath_parser_t *parser, jspath_value_t *val);
static int ab_jspath_exp_postfix(jspath_parser_t *parser, jspath_value_t *val);
static int ab_jspath_exp_atomic(jspath_parser_t *parser, jspath_value_t *val);

static int ab_jspath_init_parser(jspath_parser_t *parser, ui8_t *jsbin, ui32_t jsbin_len, ui32_t buf_len, const char* path, bool is_mkdir);
static void ab_jspath_destory_parser(jspath_parser_t *parser);
static void ab_jspath_clean_set(jspath_parser_t *parser);
static void ab_jspath_clean_tmp(jspath_parser_t *parser);
static void ab_jspath_swap_set(jspath_parser_t *parser);
static int ab_jspath_exe_child(jspath_parser_t *parser, const char *name, int name_len);
static int ab_jspath_exe_recursive(jspath_parser_t *parser, const char *name, int name_len);
static int ab_jspath_exe_index(jspath_parser_t *parser, ab_list_t *idset);
static int ab_jspath_exe_slice(jspath_parser_t *parser, int start, int end, int step);
static int ab_jspath_exe_wildcard(jspath_parser_t *parser);
static void __opr(int op, jspath_value_t *v1, jspath_value_t *v2);

//define in ab_jsonbuf.c
extern int ab_json_token_to_string(const char *begin, int len, char **str); 
extern ui8_t* ab_jsbuf_get_varint64(ui8_t* p, ui64_t* value);
extern ui32_t ab_jsbuf_encode_varint64(ui8_t* dst, ui64_t v);
extern int ab_jsbuf_update_len_func(ui8_t* buf, ui32_t *len, ui32_t buf_len, ui8_t *pos, int diff_len);
extern int ab_jsbuf_put_item(ab_jsbuf_t* jsbuf, ui8_t type, ui8_t *name, ui32_t name_len, ui8_t *value, ui32_t value_len);
extern ui8_t* ab_jsbuf_get_item_func(ui8_t* buf, ui32_t len, js_item_t *item);
extern ui8_t *ab_jsbuf_get_item(ab_jsbuf_t* jsbuf, js_item_t *item);
extern ui8_t* ab_jsbuf_get_child(ab_jsbuf_t* jsbuf);
extern ui8_t* ab_jsbuf_get_next(ab_jsbuf_t* jsbuf);

static inline bool ab_jspath_ptr_invalid(jspath_parser_t *parser, ui8_t *item)
{
    if (item < parser->jsbuf.buf || item >= parser->jsbuf.buf + parser->jsbuf.size) return false;
    return true; 
}

static inline ab_jsbuf_t ab_jspath_ptr_to_jsbuf(jspath_parser_t *parser, ui8_t *item)
{
    ab_jsbuf_t jsbuf = {
        .buf = item,
        .current = item,
        .size = parser->jsbuf.buf + parser->jsbuf.size - item
    };
    return jsbuf;
}

static ui8_t *ab_jspath_get_item(jspath_parser_t *parser, ui8_t *item, js_item_t *obj)
{
    ab_jsbuf_t jsbuf = ab_jspath_ptr_to_jsbuf(parser, item);

    if (!ab_jspath_ptr_invalid(parser, item)) return NULL;
    return ab_jsbuf_get_item(&jsbuf, obj);
}

static ui8_t *ab_jspath_get_child(jspath_parser_t *parser, ui8_t *parent, js_item_t *obj)
{
    ui8_t *child = NULL;
    ab_jsbuf_t jsbuf = ab_jspath_ptr_to_jsbuf(parser, parent);

    if (!ab_jspath_ptr_invalid(parser, parent)) return NULL;
    if (!(child = ab_jsbuf_get_child(&jsbuf))) return NULL;
    if (!ab_jspath_get_item(parser, child, obj)) return NULL;

    if (parser->is_mkdir)
    {
        ab_hash_insert(parser->parent_map, child, 0, parent, NULL);
    }
    return child; 
}

static ui8_t *ab_jspath_get_next(jspath_parser_t *parser, ui8_t *parent, ui8_t *item, js_item_t *obj)
{
    ui8_t *next = NULL;
    ab_jsbuf_t jsbuf = ab_jspath_ptr_to_jsbuf(parser, item);

    if (!ab_jspath_ptr_invalid(parser, item)) return NULL;
    if (!(next = ab_jsbuf_get_next(&jsbuf))) return NULL;
    if (!ab_jspath_get_item(parser, next, obj)) return NULL;

    if (parser->is_mkdir)
    {
        ab_hash_insert(parser->parent_map, next, 0, parent, NULL);
    }
    return next; 
}

static int ab_jspath_add_child(jspath_parser_t *parser, ui8_t *parent, const char *name, int namelen, ui8_t**child)
{
    js_item_t item_obj = {0};
    js_item_t parent_obj = {0};
    ab_jsbuf_t jsbuf = {0};
    ui8_t tmpbuf[32] = {0};
    ui32_t tmplen = 0;
    ui8_t *item = NULL;
    ui32_t child_offset_updated = 0;
    ui32_t parent_offset_updated = 0;
    int ret;

    jsbuf.buf = jsbuf.current = tmpbuf;
    jsbuf.size = sizeof(tmpbuf);
    ab_jsbuf_put_item(&jsbuf, ABJSION_UNKNOW, (ui8_t*)name, namelen, NULL, 0);
    tmplen = jsbuf.current - jsbuf.buf;
    if (tmplen + parser->jsbuf.size > parser->jsbuf_len)
        return ab_ret_jsonbuf_no_left_space;

    //insert item
    ab_jspath_get_item(parser, parent, &parent_obj);
    *child = parent_obj.value + parent_obj.value_len;
    memmove(*child + tmplen, *child, parser->jsbuf.buf + parser->jsbuf.size - *child);
    memcpy(*child, tmpbuf, tmplen);
    
    parser->jsbuf.size += tmplen;

    //update len of parent items
    item = parent;
    child_offset_updated = parser->jsbuf.size;
    while (item)
    {
        ab_jspath_get_item(parser, item, &item_obj);
        //printf(">>>>>change len: %p %.*s\n", item, item_obj.name_len, item_obj.name);
        ret = ab_jsbuf_update_len_func(parser->jsbuf.buf, &parser->jsbuf.size, parser->jsbuf_len, item_obj.name+item_obj.name_len, tmplen); 
        if (ret != ab_ret_ok) return ret;  
        if (item == parent) parent_offset_updated = parser->jsbuf.size;

        if (item == parser->jsbuf.buf) break;
        item = ab_hash_get(parser->parent_map, item, 0, 0);
    }
    if (item != parser->jsbuf.buf)
    {
        return ab_ret_jsonpath_parent_item_lost;
    }

    //insert parent map
    child_offset_updated = parser->jsbuf.size - child_offset_updated;
    parent_offset_updated = parser->jsbuf.size - parent_offset_updated;
    ab_hash_insert(parser->parent_map, (*child + child_offset_updated), 0, (parent + child_offset_updated), NULL);
    //{
    //    js_item_t obj = {0};
    //    js_item_t par = {0};
    //    ab_jspath_get_item(parser, (*child + child_offset_updated), &obj);
    //    ab_jspath_get_item(parser, (parent + child_offset_updated), &par);
    //    printf("inset: %p %p %.*s->%.*s\n", (*child + child_offset_updated),  (parent + child_offset_updated), 
    //        obj.name_len, obj.name, par.name_len, par.name);
    //}
    
    return ab_ret_ok;
}

int ab_json_parse_path(ui8_t *jsbin, const char* path, js_item_t *set, int set_max, int *set_num) 
{
    int ret = 0;
    int i = 0;
    jspath_parser_t parser = {0};

    ret = ab_jspath_init_parser(&parser, ((ab_jsbin_t*)jsbin)->buf, ((ab_jsbin_t*)jsbin)->jsbin_len, ((ab_jsbin_t*)jsbin)->buf_size, path, false);
    if (ret != 0) return ret;

    ab_jspath_get_token(&parser);
    if (cur_token(&parser).type == TOKEN_ROOT || cur_token(&parser).type == TOKEN_CURRENT) 
    {
        ab_list_put(parser.set, parser.jsbuf.buf);
    } 
    else 
    {
        ab_jspath_destory_parser(&parser);
        return ab_return(ab_ret_jsonpath_expected_root_or_current_symbol);
    }

    ab_jspath_get_token(&parser);
    ret = ab_jspath_parse_tail(&parser);
    if (ret != 0)
    {
        ab_jspath_destory_parser(&parser);
        return ret;
    }

    for (i=0; i<set_max && set; i++)
    {
        ui8_t *js = ab_list_del(parser.set);
        if (js == NULL) break;
        if (!ab_jspath_get_item(&parser, js, &set[i]))
        {
            break;
        }
    }
    *set_num = i;

    ab_jspath_destory_parser(&parser);
    return 0;
}

static int ab_jspath_parse_tail(jspath_parser_t *parser) 
{
    int ret = 0;

    if (cur_token(parser).type == TOKEN_END) 
    {
        return 0;
    }
    
    if (cur_token(parser).type == TOKEN_RECURSIVE )
    {
        ret = ab_jspath_parse_recursive_descent(parser);
    }
    else if (cur_token(parser).type == TOKEN_CHILD )
    {
        ret = ab_jspath_parse_child_operator(parser);
    }
    else if (cur_token(parser).type == TOKEN_SUB_START) 
    {
        //filter_operator or subscript_operator or slice_operator 
        int sub_start = 1;
        int is_slice_operator = 0;
        jspath_token_t tok = {0};
        int start = parser->token_cur-1; //begin at [

        ab_jspath_get_token(parser);
        if (cur_token(parser).type == TOKEN_FILTER)
        {
            ret = ab_jspath_parse_filter_operator(parser);
        }
        else
        {
            while(sub_start > 0 && cur_token(parser).type != TOKEN_END)
            {
                if (sub_start == 1 && cur_token(parser).type == TOKEN_SLICE)
                {
                    is_slice_operator = 1;
                    break; 
                } 
                else if (cur_token(parser).type == TOKEN_SUB_START) sub_start++;
                else if (cur_token(parser).type == TOKEN_SUB_END) sub_start--;

                ab_jspath_get_token(parser);
            }
            
            parser->token_cur = start;
            ab_jspath_get_token(parser);
            if (is_slice_operator) ret = ab_jspath_parse_array_slice_operator(parser);
            else ret = ab_jspath_parse_subscript_operator(parser);
        }
    }
    else 
    {
        return ab_return(ab_ret_jsonpath_expected_tail_operator);
    }

    if (ret != 0)
    {
        return ret;
    }

    // parse left string
    if (cur_token(parser).type == TOKEN_RECURSIVE ||
        cur_token(parser).type == TOKEN_CHILD ||
        cur_token(parser).type == TOKEN_SUB_START)
    {
        return ab_jspath_parse_tail(parser);
    }

    return 0;
}

static int ab_jspath_parse_index(jspath_parser_t *parser) 
{
    int ret = 0;
    ab_list_t *idset = ab_list_init();

    for(;;)
    {
        ab_jspath_get_token(parser);
        if (cur_token(parser).type == TOKEN_NUMBER)
        {
            char *end = NULL;
            int number = strtod(cur_token(parser).lexeme, &end);
            ab_list_put(idset, (void *)(long)number);
            ab_jspath_get_token(parser);
        }
        else if (cur_token(parser).type == TOKEN_WILDCARD)
        {
            ab_jspath_exe_wildcard(parser);
            ab_jspath_get_token(parser);
            ab_list_destory(idset);
            return 0;
        }
        else if (cur_token(parser).type == TOKEN_EXP_START)
        {
            jspath_value_t val = {0};
            int number = 0;
            ret = ab_jspath_parse_expression(parser, &val);
            if (ret != 0) return ret;
            number = (int)val.number;
            ab_list_put(idset, (void *)(long)number);
        }
        else{
            ret = ab_ret_jsonpath_expected_number_wildcard_or_union;
        }

        //check union item
        if (cur_token(parser).type != TOKEN_UNION)
        {
            break;
        }
    }

    ab_jspath_exe_index(parser, idset);
    ab_list_destory(idset);
    return ab_return(ret);
}

static int ab_jspath_parse_subscript_operator(jspath_parser_t *parser) 
{
    int ret = 0;
    if (cur_token(parser).type != TOKEN_SUB_START)
    {
        ab_jspath_put_back(parser);
        return ab_return(ab_ret_jsonpath_expected_left_bracket);
    }

    if ((ret=ab_jspath_parse_index(parser)) != 0)
    {
        return ret;
    }

    if (cur_token(parser).type != TOKEN_SUB_END)
    {
        ab_jspath_put_back(parser);
        return ab_return(ab_ret_jsonpath_expected_right_bracket);
    }
    
    ab_jspath_get_token(parser);
    return 0;
}

static int ab_jspath_parse_slice_number(jspath_parser_t *parser, jspath_value_t *val) 
{
    int ret = 0;

    if (cur_token(parser).type == TOKEN_NUMBER)
    {
        char *end = NULL;
        val->number = strtod(cur_token(parser).lexeme, &end);

        ab_jspath_get_token(parser);  
        return 0;
    }
    else if(cur_token(parser).type == TOKEN_EXP_START)
    {
        ret = ab_jspath_parse_expression(parser, val);
        return ret;
    }
    else if(cur_token(parser).type == TOKEN_SLICE || cur_token(parser).type == TOKEN_SUB_END)  //empty
    {
        return 1; //has not number
    }
    else{
        ab_jspath_put_back(parser);
        return ab_return(ab_ret_jsonpath_invalid_array_slice_number);
    }

    return 0;
}   

static int ab_jspath_parse_array_slice(jspath_parser_t *parser, jspath_value_t *start, jspath_value_t *end, jspath_value_t *step) 
{
    int ret = 0;

    start->number = 0.0;           //default
    step->number = 1.0;            //default
    end->number = 99999999.0;      //default

    if (cur_token(parser).type != TOKEN_SUB_START)
    {
        ab_jspath_put_back(parser);
        return 1;
    }

    //start ':' end ':' step | start ':' end | start ':'
    ab_jspath_get_token(parser);  
    ret = ab_jspath_parse_slice_number(parser, start);
    if (ret != 0) return ret;
    if (cur_token(parser).type != TOKEN_SLICE)
    {
        return ab_return(ab_ret_jsonpath_expected_array_slice_symbol);
    }
    //check of [start ':']
    ab_jspath_get_token(parser);
    if (cur_token(parser).type == TOKEN_SUB_END)
    {   
        ab_jspath_get_token(parser);
        return 0;
    }

    //end
    ret = ab_jspath_parse_slice_number(parser, end);
    if (ret != 0) return ret;
    //check of [start ':' end]
    if (cur_token(parser).type == TOKEN_SUB_END)
    {   
        ab_jspath_get_token(parser);
        return 0;
    }
    if (cur_token(parser).type != TOKEN_SLICE)
    {
        return ab_return(ab_ret_jsonpath_expected_array_slice_symbol);
    }
    ab_jspath_get_token(parser);

    //step
    ret = ab_jspath_parse_slice_number(parser, step);
    if (ret != 0) return ret;
    if (cur_token(parser).type != TOKEN_SUB_END)
    {   
        return ab_return(ab_ret_jsonpath_expected_right_bracket);
    }
    
    ab_jspath_get_token(parser);
    return 0;
}

static int ab_jspath_parse_array_slice_operator(jspath_parser_t *parser) 
{
    jspath_value_t start = {0}, end = {0}, step = {0};

    int ret = ab_jspath_parse_array_slice(parser, &start, &end, &step);
    if (ret != 0) return ret;

    ab_jspath_exe_slice(parser, (int)start.number, (int)end.number, (int)step.number);
    return 0;
}

static int ab_jspath_parse_child_operator(jspath_parser_t *parser) 
{
    if (cur_token(parser).type != TOKEN_CHILD)
    {
        ab_jspath_put_back(parser);
        return 1;
    }

    ab_jspath_get_token(parser);
    if (cur_token(parser).type == TOKEN_ID)
    {
        ab_jspath_exe_child(parser, cur_token(parser).lexeme, cur_token(parser).len);
    }
    else if (cur_token(parser).type == TOKEN_NUMBER && !parser->is_mkdir)
    {
        ab_list_t *idset = ab_list_init();
        char *end = NULL;
        int number = strtod(cur_token(parser).lexeme, &end);
        ab_list_put(idset, (void *)(long)number);
        ab_jspath_exe_index(parser, idset);
        ab_list_destory(idset);
    }
    else{
        ab_jspath_put_back(parser);
        return ab_return(ab_ret_jsonpath_expected_identifier);
    }

    ab_jspath_get_token(parser);
    return 0;
}

static int ab_jspath_parse_recursive_descent(jspath_parser_t *parser) 
{
    if (cur_token(parser).type != TOKEN_RECURSIVE)
    {
        ab_jspath_put_back(parser);
        return ab_return(ab_ret_jsonpath_expected_recursive_operator);
    }

    ab_jspath_get_token(parser);
    if (cur_token(parser).type != TOKEN_ID)
    {
        ab_jspath_put_back(parser);
        return ab_return(ab_ret_jsonpath_expected_identifier);
    }
    ab_jspath_exe_recursive(parser, cur_token(parser).lexeme, cur_token(parser).len);

    ab_jspath_get_token(parser);
    return 0;
}

static int ab_jspath_parse_filter_operator(jspath_parser_t *parser) 
{
    int ret = 0;
    jspath_value_t skipval = {0};
    int exp_end = 0;

    if (cur_token(parser).type != TOKEN_FILTER)
    {
        ab_jspath_put_back(parser);
        return ab_return(ab_ret_jsonpath_expected_filter_operator);
    }

    ab_jspath_get_token(parser);
    if (cur_token(parser).type != TOKEN_EXP_START)
    {
        ab_jspath_put_back(parser);
        return ab_return(ab_ret_jsonpath_expected_expression);
    }
    ab_jspath_put_back(parser);         //put back (

    ui8_t *item = NULL;
    ui8_t *scan = NULL;
    js_item_t scan_obj = {0};
    while((item = ab_list_del(parser->set)) != NULL)    
    {
        scan = ab_jspath_get_child(parser, item, &scan_obj);
        while (scan != NULL)
        {
            jspath_value_t val = {0};
            jspath_parser_t new_parser = {0};
            ui32_t scan_len = scan_obj.value + scan_obj.value_len - scan;
            
            ab_jspath_init_parser(&new_parser, scan, scan_len, scan_len, NULL, parser->is_mkdir);
            new_parser.tokens = parser->tokens;
            new_parser.token_cur = parser->token_cur;
            new_parser.token_num = parser->token_num;

            ab_list_put(new_parser.set, scan);   //init set

            ab_jspath_get_token(&new_parser);    //start at (
            ret = ab_jspath_parse_expression(&new_parser, &val);
            exp_end = new_parser.token_cur;
            ab_jspath_destory_parser(&new_parser);
            if (ret != 0) return ret;
            
            //filter
            if (__is_true(&val))
            {
                ab_list_put(parser->tmp, scan);
            }
            scan = ab_jspath_get_next(parser, item, scan, &scan_obj);
        }
    }
    ab_jspath_swap_set(parser);
    
    parser->token_cur = exp_end;
    ab_jspath_get_token(parser);
    return 0;
}

static int ab_jspath_parse_expression(jspath_parser_t *parser, jspath_value_t *val) 
{   
    return ab_jspath_exp_postfix(parser, val);
}

// ||
static int ab_jspath_exp_logic_or(jspath_parser_t *parser, jspath_value_t *val)
{
    int ret = 0;

	if ((ret = ab_jspath_exp_logic_and(parser, val)) != 0){
        return ret;
    }

	while(cur_token(parser).type == TOKEN_OPR_OR){
        jspath_value_t val2 = {0}; 
		ab_jspath_get_token(parser);
		if ((ret = ab_jspath_exp_logic_and(parser, &val2)) != 0){
            return ret;
        } 

        __opr(TOKEN_OPR_OR, val, &val2);
	}

    return 0;
}

// && 
static int ab_jspath_exp_logic_and(jspath_parser_t *parser, jspath_value_t *val)
{
    int ret = 0;

    if ((ret = ab_jspath_exp_equality(parser, val)) != 0){
        return ret;
    } 

    while(cur_token(parser).type == TOKEN_OPR_AND){
        jspath_value_t val2 = {0}; 
		ab_jspath_get_token(parser);
		if ((ret = ab_jspath_exp_equality(parser, &val2)) != 0){
            return ret;
        } 

        __opr(TOKEN_OPR_AND, val, &val2);
	}

    return 0;
}

// == != 
static int ab_jspath_exp_equality(jspath_parser_t *parser, jspath_value_t *val)
{
    int ret = 0;
    int op = 0;

    if ((ret = ab_jspath_exp_relational(parser, val)) != 0){
        return ret;
    } 

	while(cur_token(parser).type == TOKEN_OPR_EQ || cur_token(parser).type == TOKEN_OPR_NE){
        jspath_value_t val2 = {0}; 
        op = cur_token(parser).type;
		ab_jspath_get_token(parser);
		if ((ret = ab_jspath_exp_relational(parser, &val2)) != 0){
            return ret;
        } 
		
        __opr(op, val, &val2);
	}

    return 0;
}

// < > <= >= 
static int ab_jspath_exp_relational(jspath_parser_t *parser, jspath_value_t *val)
{
    int ret = 0;
	int op = 0;

    if ((ret = ab_jspath_exp_additive(parser, val)) != 0){
        return ret;
    } 

	while( 
        cur_token(parser).type == TOKEN_OPR_LT ||
        cur_token(parser).type == TOKEN_OPR_LE ||
        cur_token(parser).type == TOKEN_OPR_GT ||
        cur_token(parser).type == TOKEN_OPR_GE)
    {
        jspath_value_t val2 = {0}; 
        op = cur_token(parser).type;
		ab_jspath_get_token(parser);
		if ((ret = ab_jspath_exp_additive(parser, &val2)) != 0){
            return ret;
        } 

		__opr(op, val, &val2);
	}

    return 0;
}

static int ab_jspath_exp_additive(jspath_parser_t *parser, jspath_value_t *val)
{
    int ret = 0;
	int op = 0;

    if ((ret = ab_jspath_exp_multiplicative(parser, val)) != 0){
        return ret;
    } 
	while( 
		cur_token(parser).type == TOKEN_OPR_ADD ||
        cur_token(parser).type == TOKEN_OPR_SUB )
    {
		jspath_value_t val2 = {0}; 
        op = cur_token(parser).type;
		ab_jspath_get_token(parser);
		if ((ret = ab_jspath_exp_multiplicative(parser, &val2)) != 0){
            return ret;
        }
		__opr(op, val, &val2);
	}
    return 0;
}

// Multiply or divide two factors
static int ab_jspath_exp_multiplicative(jspath_parser_t *parser, jspath_value_t *val)
{
    int ret = 0;
	int op = 0;

    if ((ret = ab_jspath_exp_postfix(parser, val)) != 0){
        return ret;
    } 
	while( 
        cur_token(parser).type == TOKEN_WILDCARD ||
        cur_token(parser).type == TOKEN_OPR_DEV ||
        cur_token(parser).type == TOKEN_OPR_MOD )
    {
        jspath_value_t val2 = {0}; 
        op = cur_token(parser).type;
		ab_jspath_get_token(parser);
		if ((ret = ab_jspath_exp_postfix(parser, &val2)) != 0){
            return ret;
        } 

        __opr(op, val, &val2);
	}

    return 0;
}


static int ab_jspath_exp_postfix(jspath_parser_t *parser, jspath_value_t *val)
{
    int ret = 0;
	int op = 0;

	if(cur_token(parser).type == TOKEN_EXP_START){
		ab_jspath_get_token(parser);
        if ((ret = ab_jspath_exp_logic_or(parser, val)) != 0){
            return ret;
        } 

        if (cur_token(parser).type != TOKEN_EXP_END)
        {
            return ab_return(ab_ret_jsonpath_expected_end_of_expression);
        }
		
		ab_jspath_get_token(parser);
	}
	else if((ret = ab_jspath_exp_atomic(parser, val)) != 0){
		return ret;
	}

    return 0;
}

static int ab_jspath_exp_atomic(jspath_parser_t *parser, jspath_value_t *val)
{
    int ret = 0;
    char *end = NULL;

	switch(cur_token(parser).type)
    {
		case TOKEN_ID:
            //TODO function call
			//exp_checkcall(parser);
            //printf("%.*s\n", cur_token(parser).len, cur_token(parser).lexeme);
			ab_jspath_get_token(parser);
            return ab_return(ab_ret_jsonpath_expected_value); 
			break;

		case TOKEN_NUMBER:
            val->type = VTNUM;
            val->number = strtod(cur_token(parser).lexeme, &end);
			ab_jspath_get_token(parser);
			break;

		case TOKEN_STRING:
            val->type = VTSTR;
			val->str = cur_token(parser).lexeme;
            val->strlen = cur_token(parser).len;
			ab_jspath_get_token(parser);
			break;
        
        case TOKEN_CURRENT:
            ab_jspath_clean_set(parser);
            ab_jspath_clean_tmp(parser);
            ab_list_put(parser->set, parser->jsbuf.buf); //put current root to set

            ab_jspath_get_token(parser);
            ret = ab_jspath_parse_tail(parser);

            js_item_t jsitem = {0};
            ui8_t *item = ab_list_get(parser->set);
            if (item != NULL && ab_jspath_get_item(parser, item, &jsitem))
            {
                //printf("------------------ %u %.*s\n", jsitem.type, jsitem.value_len, jsitem.value);
                if(jsitem.type == ABJSION_TRUE)
                {
                    val->number = 1;
                    val->type = VTNUM;
                }
                else if(jsitem.type == ABJSION_FALSE || jsitem.type == ABJSION_NULL)
                {
                    val->number = 0;
                    val->type = VTNUM;
                }
                else if(jsitem.type == ABJSION_ULONG)
                {
                    ui64_t ulong = 0;
                    ab_jsbuf_get_varint64(jsitem.value, &ulong);
                    val->number = ulong; 
                    val->type = VTNUM;
                }
                else if(jsitem.type == ABJSION_DOUBLE)
                {
                    val->number = *(double*)jsitem.value;
                    val->type = VTNUM;
                }
                else if(jsitem.type == ABJSION_STRING)
                {
                    val->str = jsitem.value;
                    val->strlen = jsitem.value_len;
                    val->type = VTSTR;
                }
                else return ab_return(ab_ret_jsonpath_expected_value);  //only support string and number
            }

            break;

		default:
            return ab_return(ab_ret_jsonpath_expected_value);
            break;
	}

	return ret;
}

static int ab_japath_parent_map_cmp(void *k1, void *k2, void *param)
{
    return k1 - k2;
}

static int ab_jspath_init_parser(jspath_parser_t *parser, ui8_t *jsbin, ui32_t jsbin_len, ui32_t buf_len, const char* path, bool is_mkdir)
{
    memset(parser, 0, sizeof(jspath_parser_t));

    parser->jsbuf.buf = jsbin;
    parser->jsbuf.current = jsbin;
    parser->jsbuf.size = jsbin_len;
    parser->jsbuf_len = buf_len;
    parser->path = path;

    parser->set = ab_list_init();
    if(parser->set == NULL)
    {
        return ab_ret_outofmemory;
    }
    parser->tmp = ab_list_init();
    if(parser->tmp == NULL)
    {
        ab_list_destory(parser->set);
        return ab_ret_outofmemory;
    }
    
    // set value, need save parent items
    parser->is_mkdir = is_mkdir;
    if (parser->is_mkdir)
    {
        parser->parent_map = ab_hash_init(0, ab_japath_parent_map_cmp, NULL, NULL);
        if(parser->parent_map == NULL)
        {
            ab_list_destory(parser->set);
            ab_list_destory(parser->tmp);
            return ab_ret_outofmemory;
        }
    }

    // token info
    parser->token_cur = -1;
    parser->token_num = 0;
    parser->tokens = NULL;
    if (path != NULL)
    {
        parser->tokens = ab_malloc(sizeof(jspath_token_t)*(strlen(path)+2));
        if(parser->tokens == NULL)
        {
            return ab_ret_outofmemory;
        }
        
        if (ab_jspath_get_all_token(parser, path) != 0)
        {
            ab_jspath_destory_parser(parser);
            return -10;
        }
    }
    
    return 0;
}

static void ab_jspath_destory_parser(jspath_parser_t *parser)
{
    if (parser->path && parser->tokens)
    {
        int i = 0;
        for (i=0; i<parser->token_num; i++)
        {
            if (parser->tokens[i].type == TOKEN_STRING) 
                ab_free((void *)parser->tokens[i].lexeme);
        }

        ab_free(parser->tokens);
    } 

    if (parser->set) ab_list_destory(parser->set);
    if (parser->tmp) ab_list_destory(parser->tmp);
    if (parser->parent_map) ab_hash_destory(parser->parent_map);

    memset(parser, 0, sizeof(jspath_parser_t));
}

static void ab_jspath_clean_set(jspath_parser_t *parser)
{
    while(ab_list_del(parser->set));
}

static void ab_jspath_clean_tmp(jspath_parser_t *parser)
{
    while(ab_list_del(parser->tmp));
}

static void ab_jspath_swap_set(jspath_parser_t *parser)
{
    ab_list_t *tmpset = parser->set;
    parser->set = parser->tmp;
    parser->tmp = tmpset;

    ab_jspath_clean_tmp(parser);
}

static int ab_jspath_exe_child(jspath_parser_t *parser, const char *name, int name_len)
{
    ui8_t *item = NULL;
    ui8_t *scan = NULL;
    ui8_t *parent = NULL;
    js_item_t item_obj = {0};
    js_item_t scan_obj = {0};
    js_item_t parent_obj = {0};
    int ret;
    
    while((item = ab_list_del(parser->set)) != NULL)
    {
        parent = item;
        ab_jspath_get_item(parser, parent, &parent_obj);
        if (parent_obj.type != ABJSION_ARRAY && parent_obj.type != ABJSION_OBJECT)    continue;
        
        scan = ab_jspath_get_child(parser, parent, &scan_obj);
        while (scan != NULL)
        {
            if (scan_obj.name && scan_obj.name_len == name_len && strncmp(scan_obj.name, name, name_len) == 0)
            {
                ab_list_put(parser->tmp, scan);
                break;
            }
            scan = ab_jspath_get_next(parser, parent, scan, &scan_obj);
        }
    }

    ab_jspath_swap_set(parser);

    // name not exist, make new item
    if (parent && parser->is_mkdir && ab_list_is_empty(parser->set))
    {
        ui8_t *child = NULL;
        js_item_t child_obj = {0};
        //parent is new item, change to object
        if (parent_obj.type == ABJSION_UNKNOW)
        {
            parent_obj.type = ABJSION_OBJECT;
            *parent = ABJSION_OBJECT;
        }
        ret = ab_jspath_add_child(parser, parent, name, name_len, &child);
        if (ret != ab_ret_ok) return ret;
        ab_list_put(parser->set, child);
    }

    return ab_ret_ok;
}

static int ab_jspath_exe_recursive_item(jspath_parser_t *parser, ui8_t *item, const char *name, int name_len)
{
    ui8_t *scan = NULL;
    js_item_t scan_obj = {0};
    js_item_t item_obj = {0};

    ab_jspath_get_item(parser, item, &item_obj);
    if (item_obj.name && item_obj.name_len == name_len && strncmp(item_obj.name, name, name_len) == 0)
    {
        ab_list_put(parser->tmp, item);
        return 0;
    }

    if ((item_obj.type == ABJSION_ARRAY || item_obj.type == ABJSION_OBJECT) && item_obj.value_len > 0)
    {
        scan = ab_jspath_get_child(parser, item, &scan_obj);
        while (scan != NULL)
        {
            ab_jspath_exe_recursive_item(parser, scan, name, name_len);
            scan = ab_jspath_get_next(parser, item, scan, &scan_obj);
        }
    }
    return 0;
}

static int ab_jspath_exe_recursive(jspath_parser_t *parser, const char *name, int name_len)
{
    ui8_t *item = NULL;

    while((item = ab_list_del(parser->set)) != NULL)
    {
        ab_jspath_exe_recursive_item(parser, item, name, name_len);
    }

    ab_jspath_swap_set(parser);
    return 0;
}

void idset_foreach(void *value, void *param)
{
    long *p = param;
    if (value == (void *)p[0]) p[1] = 1;
}

static bool ab_jspath_exe_is_in_idset(long i, ab_list_t *idset)
{
    long param[2] = {0};
    param[0] = i;
    ab_list_foreach(idset, idset_foreach, (void *)param);
    return (param[1] == 1);
}

static int ab_jspath_exe_index(jspath_parser_t *parser, ab_list_t *idset)
{
    ui8_t *item = NULL;
    ui8_t *scan = NULL;
    ui8_t *parent = NULL;
    js_item_t item_obj = {0};
    js_item_t scan_obj = {0};
    js_item_t parent_obj = {0};
    long i = 0, get = 0;
    int ret;

    while((item = ab_list_del(parser->set)) != NULL)    
    {
        parent = item;
        ab_jspath_get_item(parser, parent, &parent_obj);
        if (parent_obj.type != ABJSION_ARRAY)    continue;
        
        i = 0;
        scan = ab_jspath_get_child(parser, parent, &scan_obj);
        while (scan != NULL)
        {
            if (ab_jspath_exe_is_in_idset(i,idset))
            {
                ab_list_put(parser->tmp, scan);
                if (++get == ab_list_num(idset)) break;
            }

            scan = ab_jspath_get_next(parser, parent, scan, &scan_obj);
            i++;
        }
    }

    ab_jspath_swap_set(parser);

    // not exist, make new item
    if (parent && parser->is_mkdir && ab_list_is_empty(parser->set))
    {
        ui8_t *child = NULL;
        js_item_t child_obj = {0};
        //parent is new item, change to array
        if (parent_obj.type == ABJSION_UNKNOW){
            parent_obj.type = ABJSION_ARRAY;
            *parent = ABJSION_ARRAY;
        } 
        ret = ab_jspath_add_child(parser, parent, NULL, 0, &child);
        if (ret != ab_ret_ok) return ret;
        ab_list_put(parser->set, child);
    }
}

static int ab_jspath_exe_slice(jspath_parser_t *parser, int start, int end, int step)
{
    ui8_t *item = NULL;
    ui8_t *scan = NULL;
    js_item_t item_obj = {0};
    js_item_t scan_obj = {0};
    int i = 0, skip = 0;

    if (start < 0 || step <= 0){
        return -1;
    }

    while((item = ab_list_del(parser->set)) != NULL)    
    {
        ab_jspath_get_item(parser, item, &item_obj);
        if (item_obj.type != ABJSION_ARRAY)    continue;   //todo support object?
        
        i = 0;
        scan = ab_jspath_get_child(parser, item, &scan_obj);
        while (scan != NULL)
        {
            if (i < start){
                scan = ab_jspath_get_next(parser, item, scan, &scan_obj);
                i++;
                continue;
            }

            if (i == end){
                break;
            }

            ab_list_put(parser->tmp, scan);
            
            for (skip=0; skip<step && scan!=NULL; skip++){
                scan = ab_jspath_get_next(parser, item, scan, &scan_obj);
                i++;
            }
        }
    }

    ab_jspath_swap_set(parser);
}

static int ab_jspath_exe_wildcard(jspath_parser_t *parser)
{
    ui8_t *item = NULL;
    ui8_t *scan = NULL;
    js_item_t item_obj = {0};
    js_item_t scan_obj = {0};

    while((item = ab_list_del(parser->set)) != NULL)
    {
        ab_jspath_get_item(parser, item, &item_obj);
        if (item_obj.type != ABJSION_ARRAY)    continue;   //todo support object?
        
        scan = ab_jspath_get_child(parser, item, &scan_obj);
        while (scan != NULL)
        {
            ab_list_put(parser->tmp, scan);
            scan = ab_jspath_get_next(parser, item, scan, &scan_obj);
        }
    }

    ab_jspath_swap_set(parser);
}

static void __opr(int op, jspath_value_t *v1, jspath_value_t *v2)
{
    //printf("v1=%1.15g v2=%1.15g opr=%d\n", v1->number, v2->number, op);
    if (op == TOKEN_OPR_OR)
    {
        v1->number = ((__is_true(v1) || __is_true(v2)) ? 1 : 0);  
        v1->type = VTNUM;  
        return;
    }

    if (op == TOKEN_OPR_AND)
    {
        v1->number = ((__is_true(v1) && __is_true(v2)) ? 1 : 0);  
        v1->type = VTNUM;  
        return;
    }

    //number compare
    if (v1->type == VTNUM && v2->type == VTNUM)
    {
        switch (op)
        {
        case TOKEN_OPR_EQ:    (v1->number = ((v1->number == v2->number) ? 1 : 0));  break;
        case TOKEN_OPR_NE:    (v1->number = ((v1->number != v2->number) ? 1 : 0));  break;
        case TOKEN_OPR_LT:    (v1->number = ((v1->number <  v2->number) ? 1 : 0));  break;
        case TOKEN_OPR_LE:    (v1->number = ((v1->number <= v2->number) ? 1 : 0));  break;
        case TOKEN_OPR_GT:    (v1->number = ((v1->number >  v2->number) ? 1 : 0));  break;
        case TOKEN_OPR_GE:    (v1->number = ((v1->number >= v2->number) ? 1 : 0));  break;
        case TOKEN_OPR_ADD:   (v1->number = v1->number + v2->number);  break;
        case TOKEN_OPR_SUB:   (v1->number = v1->number - v2->number);  break;
        case TOKEN_WILDCARD:  (v1->number = (v1->number * v2->number));  break;
        case TOKEN_OPR_DEV:   (v1->number = (v1->number / v2->number));  break;
        case TOKEN_OPR_MOD:   (v1->number = ((int)v1->number % (int)v2->number));  break;
        default:
            break;
        }
        return;
    }
    
    //string opr
    if ((v1->type == VTSTR) && (v2->type == VTSTR))
    {
        const char *s1 = v1->str;
        const char *s2 = v2->str;
        int minlen = (v1->strlen < v2->strlen ? v1->strlen : v2->strlen);
        int cmp = strncmp(s1, s2, minlen);
        if (cmp == 0 && v1->strlen != v2->strlen)
        {
            cmp = v1->strlen - v2->strlen;
        }
        switch (op)
        {
        case TOKEN_OPR_EQ:    (v1->number = (cmp==0 ? 1 : 0));  v1->type = VTNUM; break;
        case TOKEN_OPR_NE:    (v1->number = (cmp!=0 ? 1 : 0));  v1->type = VTNUM; break;
        case TOKEN_OPR_LT:    (v1->number = (cmp< 0 ? 1 : 0));  v1->type = VTNUM; break;
        case TOKEN_OPR_LE:    (v1->number = (cmp<=0 ? 1 : 0));  v1->type = VTNUM; break;
        case TOKEN_OPR_GT:    (v1->number = (cmp> 0 ? 1 : 0));  v1->type = VTNUM; break;
        case TOKEN_OPR_GE:    (v1->number = (cmp>=0 ? 1 : 0));  v1->type = VTNUM; break;
        case TOKEN_OPR_ADD: 
        case TOKEN_OPR_SUB:  
        default:
            break;
        }
        return;
    }

    //diff type 
    if (v1->type != v2->type)
    {
        switch (op)
        {
        case TOKEN_OPR_EQ:    (v1->number = 0);  v1->type = VTNUM;  break;
        case TOKEN_OPR_NE:    (v1->number = 1);  v1->type = VTNUM;  break;
        case TOKEN_OPR_LT:    (v1->number = 0);  v1->type = VTNUM;  break;
        case TOKEN_OPR_LE:    (v1->number = 0);  v1->type = VTNUM;  break;
        case TOKEN_OPR_GT:    (v1->number = 0);  v1->type = VTNUM;  break;
        case TOKEN_OPR_GE:    (v1->number = 0);  v1->type = VTNUM;  break;
        case TOKEN_WILDCARD:  (v1->number = 0);  v1->type = VTNUM;  break;
        case TOKEN_OPR_DEV:   (v1->number = 0);  v1->type = VTNUM;  break;
        case TOKEN_OPR_MOD:   (v1->number = 0);  v1->type = VTNUM;  break;
        case TOKEN_OPR_ADD: 
        case TOKEN_OPR_SUB:  
        default:
            break;
        }
        return;
    }
}





typedef int (*set_func_f)(jspath_parser_t *parser, ui8_t *item, void *value, ui32_t len, ui8_t *out_buf, ui32_t *out_len);

static int ab_jspath_from_value(jspath_parser_t *parser, set_func_f set_func, ui8_t *item, void *value, ui32_t len)
{
    int diff_len = 0;
    js_item_t item_obj = {0};
    ui8_t value_buf[32] = {0};
    ui32_t buf_len = 0;
    int ret;

    if (!item || !ab_jspath_get_item(parser, item, &item_obj)) return ab_ret_invalid_ptr;

    //call set_function to encode value
    ret = set_func(parser, item, value, len, value_buf, &buf_len);
    if (ret != ab_ret_ok && ret != ab_ret_from_value_nochange) return ret;

    diff_len = (int)buf_len - (int)item_obj.value_len;
    if (diff_len > 0 && diff_len + parser->jsbuf.size > parser->jsbuf_len)
    {
        return ab_ret_jsonbuf_no_left_space;
    }
    if (diff_len != 0)
    {
        ui32_t tail_len = (parser->jsbuf.buf + parser->jsbuf.size) - (item_obj.value + item_obj.value_len);
        memmove(item_obj.value + buf_len, item_obj.value + item_obj.value_len, tail_len);
        parser->jsbuf.size = parser->jsbuf.size + diff_len; //change total len
    }

    //copy new value
    if (ret == ab_ret_from_value_nochange && len == buf_len) memcpy(item_obj.value, value, buf_len);
    else memcpy(item_obj.value, value_buf, buf_len);

    //change len of item and all parents
    while (item)
    {
        int old_len = parser->jsbuf.size;
        ab_jspath_get_item(parser, item, &item_obj);
        ret = ab_jsbuf_update_len_func(parser->jsbuf.buf, &parser->jsbuf.size, parser->jsbuf_len, item_obj.name+item_obj.name_len, diff_len);  
        if (ret != ab_ret_ok) return ret;
        diff_len += parser->jsbuf.size - old_len;

        if (item == parser->jsbuf.buf) break;
        item = ab_hash_get(parser->parent_map, item, 0, 0);
    }
    if (item != parser->jsbuf.buf)
    {
        return ab_ret_jsonpath_parent_item_lost;
    }
    //todo left len? 

    return ab_ret_ok;
}

int ab_jspath_mkdir(ui8_t *jsbin, const char* path, set_func_f set_func, void *value, ui32_t len)
{
    int ret = 0;
    ui8_t *item = NULL;
    js_item_t item_obj = {0};
    jspath_parser_t parser = {0};

    if (((ab_jsbin_t*)jsbin)->buf_size < 8) 
        return ab_ret_jsonbuf_no_left_space;

    //empty buffer
    if (((ab_jsbin_t*)jsbin)->jsbin_len == 0)
    {
        ab_jsbuf_t jsbuf = {0}; 
        jsbuf.buf = jsbuf.current = ((ab_jsbin_t*)jsbin)->buf;
        jsbuf.size = ((ab_jsbin_t*)jsbin)->buf_size;
        ab_jsbuf_put_item(&jsbuf, ABJSION_UNKNOW, (ui8_t*)"", 0, NULL, 0);
        ((ab_jsbin_t*)jsbin)->jsbin_len = jsbuf.current - jsbuf.buf;
    }
    
    ret = ab_jspath_init_parser(&parser, ((ab_jsbin_t*)jsbin)->buf, ((ab_jsbin_t*)jsbin)->jsbin_len, ((ab_jsbin_t*)jsbin)->buf_size, path, true);
    if (ret != 0) return ret;

    ab_jspath_get_token(&parser);
    if (cur_token(&parser).type != TOKEN_ROOT && cur_token(&parser).type != TOKEN_CURRENT) 
    {
        ab_jspath_destory_parser(&parser);
        return ab_return(ab_ret_jsonpath_expected_root_or_current_symbol);
    }
    ab_list_put(parser.set, parser.jsbuf.buf);

    ab_jspath_get_token(&parser);
    ret = ab_jspath_parse_tail(&parser);
    if (ret != 0)
    {
        ab_jspath_destory_parser(&parser);
        return ret;
    }

    //set value
    ret = ab_ret_jsonpath_item_not_found;
    while((item = ab_list_del(parser.set)) != NULL)
    {
        int one_ret = ab_jspath_from_value(&parser, set_func, item, value, len);
        if (one_ret != ab_ret_ok) ret = one_ret;
        if (one_ret == ab_ret_ok && ret == ab_ret_jsonpath_item_not_found) ret = ab_ret_ok;

        ((ab_jsbin_t*)jsbin)->jsbin_len = parser.jsbuf.size; //return updated length

        //todo:
        //only support one item, because postion of items in list maybe has changed after set_func called
        break;  
    }

    ab_jspath_destory_parser(&parser);
    return ab_return(ret);
}

int set_func_f_from_ulong(jspath_parser_t *parser, ui8_t *item, void *value, ui32_t len, ui8_t *out_buf, ui32_t *out_len)
{
    js_item_t item_obj = {0};
    
    if (!item || !ab_jspath_get_item(parser, item, &item_obj)) return ab_ret_invalid_ptr;
    if (item_obj.type != ABJSION_UNKNOW && item_obj.type != ABJSION_ULONG && item_obj.type != ABJSION_DOUBLE) 
        return ab_ret_jsonpath_type_conflict_long;
    
    //set type
    *item = ABJSION_ULONG;

    //set value
    *out_len = ab_jsbuf_encode_varint64(out_buf, *(ui64_t*)value);
    return ab_ret_ok;
}

int set_func_f_from_string(jspath_parser_t *parser, ui8_t *item, void *value, ui32_t len, ui8_t *out_buf, ui32_t *out_len)
{
    js_item_t item_obj = {0};
    
    if (!item || !ab_jspath_get_item(parser, item, &item_obj)) return ab_ret_invalid_ptr;
    if (item_obj.type != ABJSION_UNKNOW && item_obj.type != ABJSION_STRING) 
        return ab_ret_jsonpath_type_conflict_string;
    
    //set type
    *item = ABJSION_STRING;

    //set value
    *out_len = len;
    return ab_ret_from_value_nochange;
}

int ab_json_from_long(ui8_t *jsbin, const char* path, ui64_t v)
{
    return ab_jspath_mkdir(jsbin, path, set_func_f_from_ulong, (void *)&v, sizeof(ui64_t));
}

int ab_json_from_string(ui8_t *jsbin, const char* path, const char* v)
{
    return ab_jspath_mkdir(jsbin, path, set_func_f_from_string, (void *)v, (ui32_t)strlen(v));
}

int ab_json_to_long(ui8_t *jsbin, const char* path, ui64_t *v)
{
    js_item_t set[1] = {0};
    int num = 0;
    int err = ab_json_parse_path(jsbin, path, set, 1, &num);
    if (err != ab_ret_ok) return err;
    if (num == 0) return ab_ret_jsonpath_item_not_found;

    ab_jsbuf_get_varint64(set[0].value, v);
    return ab_ret_ok;
}

int ab_json_to_string(ui8_t *jsbin, const char* path, char* v, int v_size)
{
    js_item_t set[1] = {0};
    int num = 0;
    int err = ab_json_parse_path(jsbin, path, set, 1, &num);
    if (err != ab_ret_ok) return err;
    if (num == 0) return ab_ret_jsonpath_item_not_found;

    snprintf(v, v_size, "%.*s", set[0].value_len, set[0].value);
    return ab_ret_ok;
}

/*
 * lex, get token functions
*/

static int ab_jspath_get_all_token(jspath_parser_t *parser, const char* path) 
{
    const char* start = NULL;
    const char* lexeme = NULL;
    const char* cur = path;
    int length = 0;
    jspath_token_t token = {0};
    
    while(token.type != TOKEN_END && *cur != '\0')
    {
        //skip space
        while(*cur == ' ' || *cur == '\t') cur++;

        //string
        if (*cur == '\"')
        {
            *cur++;

            token.type = TOKEN_STRING;
            token.lexeme = cur;
            token.len = 0;

            //search end of string
            while(*cur != '\0')
            {
                if (*cur == '\\')
                {
                    if (*(cur+1) == '\0')
                    {
                        token.type = TOKEN_INVALID;
                        return ab_return(ab_ret_jsonpath_invalid_string_token);
                    }
                    cur += 2;
                    token.len += 2;
                }
                else if (*cur == '\"')
                {
                    break;
                }
                else
                {
                    cur += 1;
                    token.len += 1;
                }
            }
            if (*cur != '\"')
            {
                token.type = TOK_INVALID;
                return ab_return(ab_ret_jsonpath_invalid_string_token);
            }
            cur += 1;

            char *out = NULL;
            token.len = ab_json_token_to_string(token.lexeme, token.len, &out);
            if (token.len < 0 || out == NULL)
            {
                return -3;
            }
            token.lexeme = out;
            parser->tokens[parser->token_num++] = token;
            continue;
        }

        //number
        if (*cur >= '0' && *cur <= '9')
        {
            token.type = TOKEN_NUMBER;
            token.lexeme = cur;
            token.len = 0;

            //TODO.... 如何区分 1+1 和 1e+4
            while(*cur == '0' || *cur == '1' || *cur == '2' || *cur == '3' || *cur == '4' ||
                  *cur == '5' || *cur == '6' || *cur == '7' || *cur == '8' || *cur == '9' ||
                  *cur == '.' )
                  //*cur == '+' || *cur == 'e' || *cur == 'E' || *cur == '.' )
            {
                cur++;
                token.len++;
            }
            if (*(cur-1) == '.') 
            {
                cur--;
                token.len--;
            } 

            parser->tokens[parser->token_num++] = token;
            continue;
        }

        switch (*cur) 
        {
        case '$': cur++; token = (jspath_token_t) {TOKEN_ROOT, "$", 1}; break;
        case '@': cur++; token = (jspath_token_t) {TOKEN_CURRENT, "@", 1}; break;
        case '[': cur++; token = (jspath_token_t) {TOKEN_SUB_START, "[", 1}; break;
        case ']': cur++; token = (jspath_token_t) {TOKEN_SUB_END, "]", 1}; break;
        case '*': cur++; token = (jspath_token_t) {TOKEN_WILDCARD, "*", 1}; break;
        case ',': cur++; token = (jspath_token_t) {TOKEN_UNION, ",", 1}; break;
        case ':': cur++; token = (jspath_token_t) {TOKEN_SLICE, ":", 1}; break;
        case '?': cur++; token = (jspath_token_t) {TOKEN_FILTER, "?", 1}; break;
        case '(': cur++; token = (jspath_token_t) {TOKEN_EXP_START, "(", 1}; break;
        case ')': cur++; token = (jspath_token_t) {TOKEN_EXP_END, ")", 1}; break;

        case '.':
            cur++;
            if (*cur == '.'){
                cur++;
                token = (jspath_token_t) {TOKEN_RECURSIVE, "..", 2};
            }
            else{
                token = (jspath_token_t) {TOKEN_CHILD, ".", 1};
            }
            break;

        case '+': cur++; token = (jspath_token_t) {TOKEN_OPR_ADD, "+", 1}; break;
        case '-': cur++; token = (jspath_token_t) {TOKEN_OPR_SUB, "-", 1}; break;
        case '/': cur++; token = (jspath_token_t) {TOKEN_OPR_DEV, "/", 1}; break;
        case '%': cur++; token = (jspath_token_t) {TOKEN_OPR_MOD, "%", 1}; break;
        case '>': 
            if(*(cur+1) == '='){
                cur+=2; 
                token = (jspath_token_t) {TOKEN_OPR_GE,  ">=", 2};
            }
            else{
                cur++; 
                token = (jspath_token_t) {TOKEN_OPR_GT,  ">", 1};
            }
            break;
        case '<': 
            if(*(cur+1) == '='){
                cur+=2; 
                token = (jspath_token_t) {TOKEN_OPR_LE,  "<=", 2};
            }
            else{
                cur++; 
                token = (jspath_token_t) {TOKEN_OPR_LT,  "<", 1};
            }
            break;
        case '=':
            if(*(cur+1) == '='){
                cur+=2; 
                token = (jspath_token_t) {TOKEN_OPR_EQ,  "==", 2};
            }
            break;
        case '!':
            if(*(cur+1) == '='){
                cur+=2; 
                token = (jspath_token_t) {TOKEN_OPR_NE,  "!=", 2};
            }
            break;

        case '|':
            if(*(cur+1) == '|'){
                cur+=2; 
                token = (jspath_token_t) {TOKEN_OPR_OR,  "||", 2};
            }
            break;

        case '&':
            if(*(cur+1) == '&'){
                cur+=2; 
                token = (jspath_token_t) {TOKEN_OPR_AND,  "&&", 2};
            }
            break;
        
        case '\0':
            token = (jspath_token_t) {TOKEN_END, "", 0};
            break;
            
        default:
            start = cur;
            while (*cur !='\0' && *cur != '$' && *cur != '.' && *cur != '@' && 
                *cur != '[' && *cur != ']' && *cur != '*' && *cur != ',' && 
                *cur != ':' && *cur != '?' && *cur != '(' && *cur != ')' &&
                *cur != '+' && *cur != '-' && *cur != '/' && *cur != '=' &&
                *cur != '|' && *cur != '&' && *cur != '!' && *cur != '%' && 
                *cur != '>' && *cur != '<' && *cur != ' ' && *cur != '\t') {
                cur++;
            }
            length = cur - start;
            lexeme = start;
            token = (jspath_token_t) {TOKEN_ID, lexeme, length};
            break;
        }

        parser->tokens[parser->token_num++] = token;
    }

    parser->tokens[parser->token_num++] = (jspath_token_t) {TOKEN_END, "", 0};
    
    //{
    //    int i = 0;
    //    for (i=0; i<parser->token_num; i++)
    //    {
    //        printf("TOKEN: %02d %.*s\n", parser->tokens[i].type, parser->tokens[i].len, parser->tokens[i].lexeme);
    //    }
    //}
    
    return 0;
}

static void printf_buf(ui8_t *buf, ui32_t len)
{
    ui32_t i;
    for (i=0; i<len; i++) printf("%02x ", buf[i]);
    printf("\n");
}

#include <stdio.h>
#include <string.h>
#include <assert.h>









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


