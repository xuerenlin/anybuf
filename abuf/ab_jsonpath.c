#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include "ab_global.h"
#include "ab_error.h"
#include "ab_list.h"
#include "ab_json.h"
#include "ab_hash.h"

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
