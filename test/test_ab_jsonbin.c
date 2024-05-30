#include <string.h>
#include "CuTest.h"
#include "ab_error.h"
#include "ab_json.h"
#include "ab_mem.h"

void TEST_ab_jsonbuf(CuTest* tc) 
{
    ui8_t js_bin[1024] = {0};
    ui8_t js_str[1024] = {0};
    int i, buf_len = 0, str_len = 0;
    int ret = 0;
    
    //ab_jsbuf_test_varint();

    const char *str = "{\"a\": \"123\", \"b\":[1,2,3,1.2,-3,false,null], \"c\":true}";
    //const char *str = "{\"c\":true}";
    ab_jsbin_init(js_bin);
    ret = ab_json_parse(str, strlen(str), js_bin);
    CuAssertIntEquals(tc, 0, ret);
    for (i=0; i<buf_len; i++) printf("%02x ", js_bin[i]);
    printf("\n");

    ab_json_snprint(js_bin, 1, js_str, sizeof(js_str));
    printf("%s\n", js_str);

    CuAssertIntEquals(tc, 0, ab_mem_leak_check());
}

CuSuite* CuGetSuite(void)
{
    CuSuite* suite = CuSuiteNew();
    
    SUITE_ADD_TEST(suite, TEST_ab_jsonbuf);
	return suite;
}

int main(int argc, char** argv) 
{
    CuRunAllTests();
    return 0;
}
