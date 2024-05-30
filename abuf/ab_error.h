#ifndef __AB_ERROR__
#define __AB_ERROR__

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
