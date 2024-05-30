#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>

#include "CuTest.h"    
#include "ab_list.h"

void callback(void *value, void *buf)
{
    sprintf((char *)buf+strlen((char *)buf), "%s", (char *)value);
}

void TEST_ab_list_put(CuTest* tc) 
{
    ab_list_t *list = ab_list_init();
    char buf[512]={0};

    ab_list_put(list, (void *)"1");
    ab_list_put(list, (void *)"2");
    ab_list_put(list, (void *)"3");

    ab_list_foreach(list, callback, buf);
    CuAssertStrEquals(tc, buf, "123");
    
    ab_list_destory(list);
    printf("mem_leak:%llu\n", ab_mem_leak_check());
}

void TEST_ab_list_get(CuTest* tc) 
{
    ab_list_t *list = ab_list_init();
    char buf[512]={0};

    ab_list_put(list, (void *)"1");
    ab_list_put(list, (void *)"2");
    ab_list_put(list, (void *)"3");

    void * n1 = ab_list_del(list);
    void * n2 = ab_list_del(list);

    ab_list_foreach(list, callback, buf);
    CuAssertStrEquals(tc, buf, "3");

    void * n3 = (struct node*)ab_list_del(list);
    CuAssertTrue(tc, ab_list_del(list) == NULL);
    CuAssertStrEquals(tc, (char*)n1, "1");
    CuAssertStrEquals(tc, (char*)n2, "2");
    CuAssertStrEquals(tc, (char*)n3, "3");

    ab_list_destory(list);
    printf("mem_leak:%llu\n", ab_mem_leak_check());
}

void TEST_ab_list_isempty(CuTest* tc) 
{
    ab_list_t *list = ab_list_init();

    CuAssertTrue(tc, ab_list_is_empty(list));

    ab_list_put(list, (void*)"1");
    CuAssertTrue(tc, (ab_list_is_empty(list) == false));

    ab_list_destory(list);
    printf("mem_leak:%llu\n", ab_mem_leak_check());
}


//////////////////////////////////////////////////////////
#define TEST_CNT 100000
#define TEST_THREADS  40
ab_list_t *list = NULL;
long g_sum = 0;
long g_left = 0;
char g_values[TEST_CNT][16] = {0};
int  g_tick[TEST_CNT] = {0};

void callback2(void *value, void *buf)
{
    OS_ATOMIC_ADD(&g_sum, atoi((char*)value));
    OS_ATOMIC_ADD(&g_tick[atoi((char*)value)], 1);
    OS_ATOMIC_ADD(&g_left, 1);
}

void *test_func_put(void *arg)
{
    int i=0;
    for(i=0;i<TEST_CNT;++i)
    {
        ab_list_put(list, (void *)g_values[i]);
        //usleep(rand()%100);
    }
    return NULL;
}

void *test_func_get(void *arg)
{
    int i = 0;
    for(i=0;i<TEST_CNT;++i)
    {
        void *n = ab_list_del(list);
        //usleep(rand()%2);
        if (n == NULL)
        { 
            continue;
        }
        OS_ATOMIC_ADD(&g_sum, atoi((char*)n));
        OS_ATOMIC_ADD(&g_tick[atoi((char*)n)], 1);

        if (g_tick[atoi((char*)n)] > 10)
        {
            //printf("%s xxxxxxxxxxxx\n", n);
        }
    }
    return NULL;
}

void TEST_ab_list_nonlock(CuTest* tc) 
{
    pthread_t putid[TEST_THREADS];
    pthread_t getid[TEST_THREADS];
    int i = 0, j = 0;
    long check_sum = 0;

    list = ab_list_init();

    for (j=0; j<TEST_CNT; j++) 
    {
        sprintf(g_values[j], "%d", j);
    }

    for (i=0; i<TEST_THREADS; i++)
    {
        for (j=0; j<TEST_CNT; j++)  check_sum += j;
        pthread_create(&putid[i], NULL, test_func_put, NULL);
    }

    for(i=0; i<TEST_THREADS; i++)
    {
        pthread_create(&getid[i], NULL, test_func_get, NULL);
    }

    for(i=0;i<TEST_THREADS;++i)
    {
        pthread_join(putid[i],NULL);
        pthread_join(getid[i],NULL);
    }
    
    //将未获取的数据获取完
    ab_list_foreach(list, callback2, NULL);

    printf("g_sum = %ld, g_left = %ld/%d---------------%.02f\n", g_sum, g_left, TEST_THREADS*TEST_CNT, g_left*100.0/(TEST_THREADS*TEST_CNT));
    for (j=0; j<TEST_CNT; j++) 
    {
        if (g_tick[j] != TEST_THREADS) printf("%d get times: %d\n", j, g_tick[j]);
    }

    ab_list_destory(list);
    printf("mem_leak:%llu\n", ab_mem_leak_check());

    CuAssertTrue(tc, g_sum == check_sum);
}


CuSuite* CuGetSuite(void)
{
    CuSuite* suite = CuSuiteNew();

    //add tests
	SUITE_ADD_TEST(suite, TEST_ab_list_put);
    SUITE_ADD_TEST(suite, TEST_ab_list_get);
    SUITE_ADD_TEST(suite, TEST_ab_list_isempty);
    SUITE_ADD_TEST(suite, TEST_ab_list_nonlock);
    
	return suite;
}

int main(int argc, char** argv) 
{
    CuRunAllTests();
    return 0;
}

