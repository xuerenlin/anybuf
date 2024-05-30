#ifndef __AB_JSON__
#define __AB_JSON__

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
