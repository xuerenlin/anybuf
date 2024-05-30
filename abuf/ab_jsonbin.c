#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <math.h>
#include "ab_global.h"
#include "ab_error.h"
#include "ab_mem.h"
#include "ab_json.h"

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
