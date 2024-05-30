#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>

#include "CuTest.h"    
#include "ab_mem.h"

void TEST_ab_mem_init(CuTest* tc) 
{
    int i = 0;
    ab_mem_t *mem = ab_mem_init(32, 0);

    while(i++ < 10)
    {
        char * p = (char *)ab_mem_alloc(mem);
        printf("%p \n", p);
        ab_mem_free(mem, p);   
    }

    ab_mem_destory(mem);
    printf("mem_leak:%llu\n", ab_mem_leak_check());
}

void TEST_ab_mem_threadsafe(CuTest* tc) 
{
    int i = 0;
    ab_mem_t *mem = ab_mem_init(32, 1);

    while(i++ < 10)
    {
        char * p = (char *)ab_mem_alloc(mem);
        printf("%p \n", p);
        ab_mem_free(mem, p);   
    }

    ab_mem_destory(mem);
    printf("mem_leak:%llu\n", ab_mem_leak_check());
}

/////////////////////////////////////////
#define TEST_CNT 200000
ab_mem_t *g_mem = NULL;

void *test_func_1(void *arg)
{
    int i=0;
    for(i=0;i<TEST_CNT;++i)
    {
        char * p = (char *)ab_mem_alloc(g_mem);
        //printf("alloc: %lx, free_num=%lu\n", p, g_mem->free_num);

        //usleep(rand()%100);
        ab_mem_free(g_mem, p);

        //printf("%u %u\n", g_mem->num_per_block, g_mem->free_num);
    }
    return NULL;
}

void TEST_ab_mem_nonlock(CuTest* tc) 
{
    pthread_t id[20];
    int i = 0, j = 0;

    g_mem = ab_mem_init(32, 1);

    for (i=0; i<20; i++)
    {
        pthread_create(&id[i], NULL, test_func_1, NULL);
    }
    
    for(i=0;i<20;++i)
    {
        pthread_join(id[i],NULL);
    }
    
    ab_mem_destory(g_mem);
    ab_mem_leak_check();
}


CuSuite* CuGetSuite(void)
{
    CuSuite* suite = CuSuiteNew();

    //add tests
    SUITE_ADD_TEST(suite, TEST_ab_mem_init);
    SUITE_ADD_TEST(suite, TEST_ab_mem_threadsafe);
    SUITE_ADD_TEST(suite, TEST_ab_mem_nonlock);
    
	return suite;
}

int main(int argc, char** argv) 
{
    CuRunAllTests();
    return 0;
}
