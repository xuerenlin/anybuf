#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

#include "CuTest.h"
#include "ab_mem.h"
#include "ab_buf.h"
#include "ab_json.h"

/**
 * ab_jsbin_init 比较繁琐
 * ab_jsbin_t 如何在结构体中变长存储
 * 如何快速的作为map被引用使用
 * 如何快速的作为list被应用使用
*/

typedef struct 
{
    char name[20];
    int  age;
    char address[40];
    int val;
    ui8_t jsbin[256];
} contacts_t;

void TEST_ab_buf_base(CuTest* tc)
{
    ab_buf_t buf;
    int cnt = 0;
    contacts_t a = {"linxr", 36, "ji-5-1"};
    contacts_t b = {"wf",    37, "ji-5-2"};
    contacts_t c = {"lyh",   8,  "ji-5-3"};
    contacts_t d = {"kk",    8,  "ji-5-4"};
    contacts_t e = {"linxr", 35, "ji-5-5"};
    contacts_t f = {"f",     37, "ji-5-6"};
    contacts_t g = {"g",     8,  "ji-5-7"};
    contacts_t h = {"h",     8,  "ji-5-8"};

    ab_jsbin_init(a.jsbin);
    ab_jsbin_init(b.jsbin);
    ab_jsbin_init(c.jsbin);
    ab_jsbin_init(d.jsbin);
    ab_jsbin_init(e.jsbin);
    ab_jsbin_init(f.jsbin);
    ab_jsbin_init(g.jsbin);
    ab_jsbin_init(h.jsbin);

    ab_json_from_string(a.jsbin, "$.key", "a");
    ab_json_from_string(b.jsbin, "$.key", "b");
    ab_json_from_string(c.jsbin, "$.key", "c");
    ab_json_from_string(d.jsbin, "$.key", "d");
    ab_json_from_string(d.jsbin, "$.value", "value-of-d");
    ab_json_from_string(d.jsbin, "$.desc", "d");
    ab_json_from_string(e.jsbin, "$.key", "e");
    ab_json_from_string(f.jsbin, "$.key", "d");
    ab_json_from_string(f.jsbin, "$.value", "value-of-f");
    ab_json_from_string(f.jsbin, "$.desc", "f");
    ab_json_from_string(g.jsbin, "$.key", "g");
    ab_json_from_string(h.jsbin, "$.key", "h");

    ab_buf_init_var(&buf, contacts_t, jsbin);
    ab_buf_attach_pk(&buf,    contacts_t, name,    type_str);
    ab_buf_attach_index(&buf, contacts_t, age,     type_num);
    ab_buf_attach_index(&buf, contacts_t, address, type_str);
    ab_buf_attach_index(&buf, contacts_t, jsbin,   "$.key");

    //put testing values
    CuAssertTrue(tc, ab_buf_put(&buf, &a) == 0); 
    CuAssertTrue(tc, ab_buf_put(&buf, &b) == 0); 
    CuAssertTrue(tc, ab_buf_put(&buf, &c) == 0); 
    CuAssertTrue(tc, ab_buf_put(&buf, &d) == 0); 
    CuAssertTrue(tc, ab_buf_put(&buf, &e) != 0); 
    CuAssertTrue(tc, ab_buf_put(&buf, &f) == 0); 
    CuAssertTrue(tc, ab_buf_put(&buf, &g) == 0); 
    CuAssertTrue(tc, ab_buf_put(&buf, &h) == 0); 

    //get by pk
    contacts_t of = {0};
    cnt = ab_buf_get(&buf, "f", 0, &of, 1);
    CuAssertTrue(tc, cnt == 1);
    CuAssertStrEquals(tc, of.address, f.address);

    //get by index
    contacts_t oc[5] = {0};
    int age = 8;
    cnt = ab_buf_get(&buf, &age, 1, &oc, 5);
    CuAssertTrue(tc, cnt == 4);
    CuAssertStrEquals(tc, oc[0].address, c.address);
    CuAssertStrEquals(tc, oc[1].address, d.address);
    CuAssertStrEquals(tc, oc[2].address, g.address);
    CuAssertStrEquals(tc, oc[3].address, h.address);

    ui8_t jsbin[128] = {0};
    ab_jsbin_init(jsbin);
    ab_json_from_string(jsbin, "$.key", "d");
    cnt = ab_buf_get(&buf, jsbin, 3, &oc, 5);
    CuAssertTrue(tc, cnt == 2);
    ab_json_print(oc[0].jsbin, 1);
    ab_json_print(oc[1].jsbin, 1);

    //delete by pk
    contacts_t of2 = {0};
    cnt = ab_buf_del(&buf, "f", 0, &of2, 1);
    CuAssertTrue(tc, cnt == 1);
    CuAssertStrEquals(tc, of2.address, f.address);

    cnt = ab_buf_del(&buf, "h", 0, &of2, 2);    //test top overflow
    CuAssertTrue(tc, cnt == 1);
    CuAssertStrEquals(tc, of2.address, h.address);

    //delete by index
    contacts_t ox[3] = {0};
    int age8 = 8;
    cnt = ab_buf_del(&buf, &age8, 1, &ox, 3);
    CuAssertTrue(tc, cnt == 3);
    CuAssertStrEquals(tc, ox[0].address, c.address);
    CuAssertStrEquals(tc, ox[1].address, d.address);
    CuAssertStrEquals(tc, ox[2].address, g.address);

    //left 2 nodes
    CuAssertTrue(tc, ab_buf_num(&buf) == 2);
    CuAssertTrue(tc, ab_buf_is_empty(&buf) == false);
    
    ab_buf_destory(&buf);
    CuAssertTrue(tc, ab_mem_leak_check() == 0);
}

void TEST_ab_buf_list(CuTest* tc)
{
    ab_buf_t list;
    int cnt = 0;
    contacts_t a = {"linxr", 36, "ji-5-1"};
    contacts_t b = {"wf",    37, "ji-5-2"};
    contacts_t c = {"lyh",   8,  "ji-5-3"};
    contacts_t d = {"kk",    8,  "ji-5-4"};
    contacts_t e = {"linxr", 35, "ji-5-5"};
    contacts_t f = {"f",     37, "ji-5-6"};
    contacts_t g = {"g",     8,  "ji-5-7"};
    contacts_t h = {"h",     8,  "ji-5-8"};

    ab_buf_init_fix(&list, contacts_t);
    ab_buf_attach_list(&list, contacts_t, name);

    //put testing values
    CuAssertTrue(tc, ab_buf_put(&list, &a) == 0);
    CuAssertTrue(tc, ab_buf_put(&list, &b) == 0);
    CuAssertTrue(tc, ab_buf_put(&list, &c) == 0);
    CuAssertTrue(tc, ab_buf_put(&list, &d) == 0);
    CuAssertTrue(tc, ab_buf_put(&list, &e) == 0);
    CuAssertTrue(tc, ab_buf_put(&list, &f) == 0);
    CuAssertTrue(tc, ab_buf_put(&list, &g) == 0);
    CuAssertTrue(tc, ab_buf_put(&list, &h) == 0);

    //get 
    contacts_t of = {0};
    cnt = ab_buf_get(&list, NULL, 0, &of, 1);
    CuAssertTrue(tc, cnt == 1);
    CuAssertStrEquals(tc, of.address, a.address);
    //get again
    cnt = ab_buf_get(&list, NULL, 0, &of, 1);
    CuAssertTrue(tc, cnt == 1);
    CuAssertStrEquals(tc, of.address, a.address);

    //delete
    contacts_t ox[20] = {0};
    cnt = ab_buf_del(&list, NULL, 0, &ox, 20);
    CuAssertTrue(tc, cnt == 8);
    CuAssertStrEquals(tc, ox[0].address, a.address);
    CuAssertStrEquals(tc, ox[1].address, b.address);
    CuAssertStrEquals(tc, ox[2].address, c.address);
    CuAssertStrEquals(tc, ox[7].address, h.address);
    CuAssertTrue(tc, ab_buf_del(&list, NULL, 0, &ox, 1) == 0);

    //left 0 nodes
    CuAssertTrue(tc, ab_buf_num(&list) == 0);
    CuAssertTrue(tc, ab_buf_is_empty(&list) == true);

    ab_buf_destory(&list);
    CuAssertTrue(tc, ab_mem_leak_check() == 0);
}

////////////////////////////////////// concurrent testing
#define TEST_CNT 300000
long g_put_sum = 0;
long g_get_sum = 0;

ab_buf_t c_dir;

void *test_func_put(void *arg)
{
    int i=0;

    for(i=0;i<TEST_CNT;++i)
    {
        contacts_t a = {0};
        sprintf(a.name, "n%u", i);
        a.age = 35;
        sprintf(a.address, "a%u", i%10);
        a.val = i;

        //ab_jsbin_init(a.jsbin);
        //ab_json_from_long(a.jsbin, "$.key", i);
        
        ab_buf_put(&c_dir, &a);
        OS_ATOMIC_ADD(&g_put_sum, a.val);
    }
    printf("put thread finish and exit, put %u items\n", TEST_CNT);
    return NULL;
}

void *test_func_get_by_name(void *arg)
{
    int i = 0;
    int count = 0;
    for(i=0;i<TEST_CNT;++i)
    {
        contacts_t co = {0};
        char key[32] = {0};
        sprintf(key, "n%u", i);

        if(ab_buf_del(&c_dir, key, 0, &co, 1) == 1)
        {
            OS_ATOMIC_ADD(&g_get_sum, co.val);
            count++;
            //printf("del by name: %d\n", co.val);
        }
    }
    printf("get thread(by_name) finish and exit, get %d items\n", count);
    return NULL;
}

void *test_func_get_by_address(void *arg)
{
    int i = 0;
    int j = 0;
    int count;
    for(i=0;i<TEST_CNT;++i)
    {
        int cnt = 0;
        contacts_t co[10] = {0};
        char key[32] = {0};
        sprintf(key, "a%u", i%10);

        cnt = ab_buf_del(&c_dir, key, 2, co, 10);
        if(cnt >= 1)
        {
            for(j=0; j<cnt; j++) 
            {
                OS_ATOMIC_ADD(&g_get_sum, co[j].val);
                count++;
                //ab_json_print(co[j].jsbin, 1);
                //printf("del by addr: %d : %d-%s\n", co[j].val, j, co[j].address);
            }
        }
    }
    printf("get thread(by_address) finish and exit, get %d items\n", count);
    return NULL;
}

void *test_func_get_by_age1(void *arg)
{
    int i = 0;
    int count = 0;
    for(i=0;i<TEST_CNT;++i)
    {
        contacts_t co = {0};
        int age = 35;
        if (ab_buf_del(&c_dir, &age, 1, &co, 1) == 1)   //delete 1 once time
        {
            OS_ATOMIC_ADD(&g_get_sum, co.val);
            count++;
            //printf("del by age1: %d\n", co.val);
        }
    }
    printf("get thread(by_age1) finish and exit, get %d items\n", count);
    return NULL;
}

void *test_func_get_by_age2(void *arg)
{
    int i = 0;
    int cnt = 0, c=0;
    int max = 0;
    int count = 0;
    for(i=0;i<TEST_CNT;++i)
    {
        contacts_t co[100] = {0};
        int age = 35;
        cnt = ab_buf_del(&c_dir, &age, 1, co, 100); //delete max 100 once time
        if (cnt >= 1)
        {
            if (max < cnt) max = cnt;
            for(c=0; c<cnt; c++)
            {
                OS_ATOMIC_ADD(&g_get_sum, co[c].val);
                count++;
            }
        }
    }
    printf("delete max=%d items at once\n", max);
    printf("get thread(by_age2) finish and exit, get %d items\n", count);
    return NULL;
}

void *test_func_get_by_jsbinkey(void *arg)
{
    int i = 0;
    int count = 0;
    for(i=0;i<TEST_CNT;++i)
    {
        contacts_t co = {0};
        ab_jsbin_init(co.jsbin);
        ab_json_from_long(co.jsbin, "$.key", i);

        if(ab_buf_del(&c_dir, co.jsbin, 3, &co, 1) == 1)
        {
            OS_ATOMIC_ADD(&g_get_sum, co.val);
            count++;
            //printf("del by jsbinkey: %d\n", co.val);
        }
    }
    printf("get thread(by_jsbinkey) finish and exit, get %d items\n", count);
    return NULL;
}

void TEST_ab_buf_nonlock(CuTest* tc) 
{
    pthread_t thids[10];
    int i = 0, j = 0;

    ab_buf_init_var(&c_dir, contacts_t, jsbin);
    ab_buf_attach_pk(&c_dir,    contacts_t, name,    type_str);
    ab_buf_attach_index(&c_dir, contacts_t, age,     type_num);
    ab_buf_attach_index(&c_dir, contacts_t, address, type_str);
    //ab_buf_attach_index(&c_dir, contacts_t, jsbin,   "$.key");

    pthread_create(&thids[0], NULL, test_func_put, NULL);
    usleep(1000);
    
    pthread_create(&thids[1], NULL, test_func_get_by_name, NULL);
    pthread_create(&thids[2], NULL, test_func_get_by_age1, NULL);
    pthread_create(&thids[3], NULL, test_func_get_by_age2, NULL);
    pthread_create(&thids[4], NULL, test_func_get_by_address, NULL);
    //pthread_create(&thids[5], NULL, test_func_get_by_jsbinkey, NULL);
    
    pthread_join(thids[0], NULL);
    pthread_join(thids[1], NULL);
    pthread_join(thids[2], NULL);
    pthread_join(thids[3], NULL);
    pthread_join(thids[4], NULL);
    //pthread_join(thids[5], NULL);

    pthread_create(&thids[4], NULL, test_func_get_by_name, NULL);
    pthread_join(thids[4], NULL);

    printf("g_put_sum=%lu g_get_sum=%lu\n", g_put_sum, g_get_sum);
    CuAssertTrue(tc, g_put_sum == g_get_sum);
    
    ab_buf_destory(&c_dir);
    CuAssertTrue(tc, ab_mem_leak_check() == 0);
}


CuSuite* CuGetSuite(void)
{
    CuSuite* suite = CuSuiteNew();

    //add tests
    SUITE_ADD_TEST(suite, TEST_ab_buf_base);
    SUITE_ADD_TEST(suite, TEST_ab_buf_list);
    SUITE_ADD_TEST(suite, TEST_ab_buf_nonlock);
    
	return suite;
}

int main(int argc, char** argv) 
{
    CuRunAllTests();
    return 0;
}
