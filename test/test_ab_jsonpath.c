#include <string.h>
#include "CuTest.h"
#include "ab_error.h"
#include "ab_json.h"
#include "ab_mem.h"

const char *g_json_str = \
    "{"\
    "\"firstName\": \"John\","\
    "\"lastName\": \"doe\","\
    "\"age\": 26,"\
    "\"address\": {"\
    "    \"streetAddress\": \"naist street\","\
    "    \"city\": \"Nara\","\
    "    \"postalCode\": \"630-0192\""\
    "},"\
    "\"phoneNumbers\": ["\
    "    {"\
    "        \"type\": \"iPhone\","\
    "        \"number\": \"0123-4567-8880\","\
    "        \"active\": 56"\
    "    },"\
    "    {"\
    "        \"type\": \"home\","\
    "        \"number\": \"0123-4567-8881\","\
    "        \"active\": 1.2"\
    "    },"\
    "    {"\
    "        \"type\": \"wifi\","\
    "        \"number\": \"0123-4567-8882\","\
    "        \"active\": 2.3"\
    "    }"\
    "]   "\
    "}";

struct test_inputs
{
    const char *path;
    int expect_set_idx;
    const char *expect;
    int expect_fail;
}g_test_inputs[] = {
    //test root
    {"$", 0, "{\"firstName\":\"John\",\"lastName\":\"doe\",\"age\":26,\"address\":{\"streetAddress\":\"naist street\",\"city\":\"Nara\",\"postalCode\":\"630-0192\"},\"phoneNumbers\":[{\"type\":\"iPhone\",\"number\":\"0123-4567-8880\",\"active\":56},{\"type\":\"home\",\"number\":\"0123-4567-8881\",\"active\":1.2},{\"type\":\"wifi\",\"number\":\"0123-4567-8882\",\"active\":2.3}]}", 0},
    //test child opr
    {"$.firstName", 0, "\"John\"", 0},
    {"$.age", 0, "26"},
    {"$.address", 0, "{\"streetAddress\":\"naist street\",\"city\":\"Nara\",\"postalCode\":\"630-0192\"}", 0},
    {"$.address.streetAddress", 0, "\"naist street\"", 0},
    {"$.phoneNumbers.2.active", 0, "2.3", 0},
    {"$.@", 0, NULL, ab_ret_jsonpath_expected_identifier},
    {"$.1", -1, "", 0},  
    //test recursive opr
    {"$..type", 0, "\"iPhone\"", 0},
    {"$..type", 1, "\"home\"", 0},
    {"$..type", 2, "\"wifi\"", 0},
    {"$..123", 0, NULL, ab_ret_jsonpath_expected_identifier},
    //test slice opr  
    //start ':' end ':' step 
    {"$..phoneNumbers[0:3:1].number", 0, "\"0123-4567-8880\"", 0},
    {"$..phoneNumbers[0:3:1].number", 1, "\"0123-4567-8881\"", 0},
    {"$..phoneNumbers[0:3:1].number", 2, "\"0123-4567-8882\"", 0},
    {"$..phoneNumbers[0:3:2].number", 0, "\"0123-4567-8880\"", 0},
    {"$..phoneNumbers[0:3:2].number", 1, "\"0123-4567-8882\"", 0},
    //start ':' end 
    {"$..phoneNumbers[0:2].number", 0, "\"0123-4567-8880\"", 0},
    {"$..phoneNumbers[0:2].number", 1, "\"0123-4567-8881\"", 0},
    //start ':'
    {"$..phoneNumbers[0:].number", 2, "\"0123-4567-8882\"", 0},
    //errors
    {"$..phoneNumbers[0:,].number", 0, NULL, ab_ret_jsonpath_invalid_array_slice_number},
    {"$..phoneNumbers[0:2,].number", 0, NULL, ab_ret_jsonpath_expected_array_slice_symbol},
    //test exp in slice opr
    {"$..phoneNumbers[(0+0):(1+2):(1+1)].number", 1, "\"0123-4567-8882\"", 0},
    //test index opr
    {"$..phoneNumbers[0].number", 0, "\"0123-4567-8880\"", 0},
    {"$..phoneNumbers[1].number", 0, "\"0123-4567-8881\"", 0},
    {"$..phoneNumbers[0,1,2].number", 0, "\"0123-4567-8880\"", 0},
    {"$..phoneNumbers[0,1,2].number", 1, "\"0123-4567-8881\"", 0},
    {"$..phoneNumbers[0,1,2].number", 2, "\"0123-4567-8882\"", 0},
    {"$..phoneNumbers[*].number", 0, "\"0123-4567-8880\"", 0},
    {"$..phoneNumbers[*].number", 1, "\"0123-4567-8881\"", 0},
    {"$..phoneNumbers[*].number", 2, "\"0123-4567-8882\"", 0},
    //test exp in index opr
    {"$..phoneNumbers[(0 + 1),(1 + 1)].number", 0, "\"0123-4567-8881\"", 0},
    {"$..phoneNumbers[(0 + 1),(1 + 1)].number", 1, "\"0123-4567-8882\"", 0},
    //test filter
    {"$..phoneNumbers[?(@.type == \"iPhone\")].number", 0, "\"0123-4567-8880\"", 0},
    {"$..phoneNumbers[?(@.type)].number", 0, "\"0123-4567-8880\"", 0},
    //test normal expressions
    {"$..phoneNumbers[?((1==0) || (2==1))].active", -1, "", 0},
    {"$..phoneNumbers[?((1==0) || (@.active == 2.3))].active", 0, "2.3", 0},
    {"$..phoneNumbers[?(@.active == 56)].active", 0, "56", 0},
    {"$..phoneNumbers[?((@.active == 2.3) && (3!=4) && (1<2) && (2>1.2) && (1<=1) && (12%10>=2) && (1+9-1-3*4/2 == 3))].active", 0, "2.3", 0},
    
};


int test_jsonpath_expect(CuTest* tc, ui8_t *jbin, const char *path, int i, const char *expect, int expect_fail)
{
    js_item_t set[20] = {0};
    char outs[2048] = {0};
    int num = 0;
    int err = ab_json_parse_path(jbin, path, set, 20, &num);
    printf("ret = %d\n", err);
    if (expect)
    {
        if (i < 0) {    //empty set
            CuAssertTrue(tc, num == 0);
        }
        else{
            char sub_jsbin[2048] = {0};
            CuAssertTrue(tc, num > i);

            ab_jsbin_init(sub_jsbin);
            memcpy(((ab_jsbin_t*)sub_jsbin)->buf, set[i].orgbuf, 1024);
            ((ab_jsbin_t*)sub_jsbin)->jsbin_len = 1024;  //len? toto
            ab_json_snprint(sub_jsbin, 0, outs, sizeof(outs));    
            CuAssertStrEquals(tc, outs, expect);
        }
    }
    if (expect_fail != 0)
    {
        CuAssertTrue(tc, num <= i);
        CuAssertIntEquals(tc, err, expect_fail);
    }

    return num;
}

void TEST_ab_json_path(CuTest* tc) 
{
    int i = 0;
    char jstr[2048] = {0};
    ui8_t jbin[2048] = {0};
    int ret = 0;
    
    ab_jsbin_init(jbin);
    ret = ab_json_parse(g_json_str, strlen(g_json_str), jbin);
    ab_json_snprint(jbin, 1, jstr, sizeof(jstr));
    printf("test json = \n%s\n", jstr);

    for(i=0; i<sizeof(g_test_inputs)/sizeof(struct test_inputs); i++)
    {
        const char *str = (g_test_inputs[i].expect ? g_test_inputs[i].expect : ab_error_desc(g_test_inputs[i].expect_fail));
        printf("TEST_%02d] %s => %d|%s|", i, g_test_inputs[i].path, g_test_inputs[i].expect_set_idx, str);
        int num = test_jsonpath_expect(tc, jbin, g_test_inputs[i].path, 
            g_test_inputs[i].expect_set_idx, g_test_inputs[i].expect, g_test_inputs[i].expect_fail);
    }

    CuAssertTrue(tc, ab_mem_leak_check() == 0);
}

void TEST_ab_json_path2(CuTest* tc) 
{
    int i = 0;
    char jbin[2048] = {0};
    ui64_t v = 0;
    char sv[32];
    int ret = 0;
    
    ab_jsbin_init(jbin);
    ret = ab_json_parse(g_json_str, strlen(g_json_str), jbin);
    
    ab_json_to_long(jbin, "$.age", &v);
    ab_json_to_string(jbin, "$.address.city", sv, sizeof(sv)-1);
    
    CuAssertTrue(tc, v == 26);
    CuAssertStrEquals(tc, sv, "Nara");
    CuAssertTrue(tc, ab_mem_leak_check() == 0);
}


void TEST_ab_jspath_mkdir(CuTest* tc) 
{
    char jstr[2048] = "{\"person\":{\"age\":{\"good\":1, \"day\":\"abcdefg\"}}}";
    char jbin[2048] = {0};
    int ret = 0;
    
    ab_jsbin_init(jbin);
    ret = ab_json_parse(jstr, strlen(jstr), jbin);
    printf("ab_json_parse ret=%d\n", ret);
    ab_json_print(jbin, 1);

    ret = ab_json_from_long(jbin, "$.person.age.good", 1233445455);
    ret = ab_json_from_long(jbin, "$.funny", 123);
    printf("ab_json_from_long ret=%d\n", ret);
    jstr[0] = 0;
    ab_json_snprint(jbin, 1, jstr, sizeof(jstr));
    printf("changed json = \n%s\n", jstr);


    ret = ab_json_from_string(jbin, "$.person.age.day", "hello");
    printf("ab_json_from_string ret=%d\n", ret);
    jstr[0] = 0;
    ab_json_snprint(jbin, 1, jstr, sizeof(jstr));
    printf("changed json = \n%s\n", jstr);

    ret = ab_json_from_string(jbin, "$.person.age.long", "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890");
    printf("ab_json_from_string ret=%d\n", ret);
    jstr[0] = 0;
    ab_json_snprint(jbin, 1, jstr, sizeof(jstr));
    printf("changed json = \n%s\n", jstr);

    ret = ab_json_from_string(jbin, "$.person.age.six", "666");
    printf("ab_json_from_string ret=%d\n", ret);

    ret = ab_json_from_string(jbin, "$.person.age.long", "abc");
    printf("ab_json_from_string ret=%d\n", ret);
    jstr[0] = 0;
    ab_json_snprint(jbin, 1, jstr, sizeof(jstr));
    printf("changed json = \n%s\n", jstr);

    ret = ab_json_from_string(jbin, "$.a.b.c", "abc");
    printf("ab_json_from_string ret=%d\n", ret);
    jstr[0] = 0;
    ab_json_snprint(jbin, 1, jstr, sizeof(jstr));
    printf("changed json = \n%s\n", jstr);

    ret = ab_json_from_long(jbin, "$.a.b.d[0]", 1);
    ret = ab_json_from_long(jbin, "$.a.b.d[1]", 2);
    ret = ab_json_from_string(jbin, "$.a.b.d[3]", "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890");
    printf("ab_json_from_string ret=%d\n", ret);
    jstr[0] = 0;
    ab_json_snprint(jbin, 1, jstr, sizeof(jstr));
    printf("changed json = \n%s\n", jstr);

    ret = ab_json_from_long(jbin, "$.a.b.d[1]", 2);
    printf("ab_json_from_string ret=%d\n", ret);
    jstr[0] = 0;
    ab_json_snprint(jbin, 1, jstr, sizeof(jstr));
    printf("changed json = \n%s\n", jstr);

    CuAssertTrue(tc, ab_mem_leak_check() == 0);
}

void TEST_ab_jspath_mkdir2(CuTest* tc) 
{
    char jbin[2048] = {0};
    ui64_t v = 0;
    ab_jsbin_init(jbin);
    ab_json_from_long(jbin, "$", 123);
    ab_json_print(jbin, 1);
    ab_json_to_long(jbin, "$", &v);
    CuAssertIntEquals(tc, v, 123);
    
}

void TEST_ab_jspath_mkdir3(CuTest* tc) 
{
    char jbin[2048] = {0};
    int jbin_len = 0;
    ui64_t v = 0;
    ab_jsbin_init(jbin);
    ab_json_from_long(jbin, "$.node", 123);
    ab_json_print(jbin, 1);
    ab_json_to_long(jbin, "$.node", &v);
    CuAssertIntEquals(tc, v, 123);
    
}

void TEST_ab_jspath_mkdir4(CuTest* tc) 
{
    char jbin[2048] = {0};
    int jbin_len = 0;
    char str[16]  = {0};
    ui64_t v = 0;

    ab_jsbin_init(jbin);
    ab_json_from_long(jbin, "$[0]", 123);
    ab_json_from_string(jbin, "$[1]", "hello");
    ab_json_from_long(jbin, "$[2]", 456);

    ab_json_print(jbin, 1);

    ab_json_to_long(jbin, "$[0]", &v);
    ab_json_to_string(jbin, "$[1]", str, sizeof(str));
    CuAssertIntEquals(tc, v, 123);
    CuAssertStrEquals(tc, str, "hello");
    
}

CuSuite* CuGetSuite(void)
{
    CuSuite* suite = CuSuiteNew();

    //test json path
    SUITE_ADD_TEST(suite, TEST_ab_json_path);
    SUITE_ADD_TEST(suite, TEST_ab_json_path2);
    
    SUITE_ADD_TEST(suite, TEST_ab_jspath_mkdir);
    SUITE_ADD_TEST(suite, TEST_ab_jspath_mkdir2);
    SUITE_ADD_TEST(suite, TEST_ab_jspath_mkdir3);
    SUITE_ADD_TEST(suite, TEST_ab_jspath_mkdir4);
    
	return suite;
}

int main(int argc, char** argv) 
{
    CuRunAllTests();
    return 0;
}
