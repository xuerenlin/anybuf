#include <stdio.h>
#include <string.h>
#include "ab_os.h"
#include "ab_error.h"

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
