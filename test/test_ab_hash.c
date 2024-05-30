#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>

#include "CuTest.h"    
#include "ab_hash.h"

void callback(void *key, ui32_t hashnr, void *buf)
{
    printf("%08x %s\n", hashnr, (char *)key);
}

void TEST_ab_hash_insert(CuTest* tc) 
{
    ab_hash_t *hash = ab_hash_init(0, NULL, NULL, NULL);
    char buf[512]={0};

    ab_hash_insert(hash, (void *)"1", 1, "1a", NULL);
    ab_hash_insert(hash, (void *)"2", 1, "2a", NULL);
    ab_hash_insert(hash, (void *)"3", 1, "3a", NULL);
    ab_hash_insert(hash, (void *)"4", 1, "4a", NULL);
    ab_hash_insert(hash, (void *)"5", 1, "5a", NULL);
    ab_hash_insert(hash, (void *)"6", 1, "6a", NULL);
    ab_hash_insert(hash, (void *)"7", 1, "7a", NULL);
    ab_hash_insert(hash, (void *)"8", 1, "8a", NULL);

    CuAssertStrEquals(tc, ab_hash_get(hash, "1", 1, 0), "1a");
    CuAssertStrEquals(tc, ab_hash_get(hash, "2", 1, 0), "2a");
    CuAssertStrEquals(tc, ab_hash_get(hash, "3", 1, 0), "3a");
    CuAssertStrEquals(tc, ab_hash_get(hash, "4", 1, 0), "4a");
    CuAssertStrEquals(tc, ab_hash_get(hash, "5", 1, 0), "5a");
    CuAssertStrEquals(tc, ab_hash_get(hash, "6", 1, 0), "6a");
    CuAssertStrEquals(tc, ab_hash_get(hash, "7", 1, 0), "7a");
    CuAssertStrEquals(tc, ab_hash_get(hash, "8", 1, 0), "8a");
    CuAssertStrEquals(tc, ab_hash_get(hash, "9", 1, 0), NULL);

    //ab_hash_foreach(hash, callback, buf);
    //printf("%s\n", buf);
    printf("%lld\n", ab_hash_num(hash));
    
    ab_hash_destory(hash);
    printf("mem_leak:%llu\n", ab_mem_leak_check());
}

void TEST_ab_hash_delete(CuTest* tc) 
{
    ab_hash_t *hash = ab_hash_init(0, NULL, NULL, NULL);
    char buf[512]={0};

    ab_hash_insert(hash, (void *)"1", 1, "1a", NULL);
    ab_hash_insert(hash, (void *)"2", 1, "2a", NULL);
    ab_hash_insert(hash, (void *)"3", 1, "3a", NULL);
    ab_hash_insert(hash, (void *)"4", 1, "4a", NULL);
    ab_hash_insert(hash, (void *)"5", 1, "5a", NULL);
    ab_hash_insert(hash, (void *)"6", 1, "6a", NULL);
    ab_hash_insert(hash, (void *)"7", 1, "7a", NULL);
    ab_hash_insert(hash, (void *)"8", 1, "8a", NULL);

    CuAssertStrEquals(tc, ab_hash_delete(hash, "1", 1, 0), "1a");
    CuAssertStrEquals(tc, ab_hash_delete(hash, "2", 1, 0), "2a");
    CuAssertStrEquals(tc, ab_hash_delete(hash, "3", 1, 0), "3a");
    CuAssertStrEquals(tc, ab_hash_delete(hash, "4", 1, 0), "4a");
    CuAssertStrEquals(tc, ab_hash_delete(hash, "5", 1, 0), "5a");
    CuAssertStrEquals(tc, ab_hash_delete(hash, "6", 1, 0), "6a");
    CuAssertStrEquals(tc, ab_hash_delete(hash, "7", 1, 0), "7a");
    CuAssertStrEquals(tc, ab_hash_delete(hash, "8", 1, 0), "8a");
    CuAssertStrEquals(tc, ab_hash_delete(hash, "9", 1, 0), NULL);

    //ab_hash_foreach(hash, callback, buf);
    //printf("%s\n", buf);
    printf("%lld\n", ab_hash_num(hash));
    ab_hash_destory(hash);
    printf("mem_leak:%llu\n", ab_mem_leak_check());
}


#define TEST_N  500000

void TEST_ab_hash_more_keys(CuTest* tc) 
{
    ab_hash_t *hash = ab_hash_init(0, NULL, NULL, NULL);
    char key[TEST_N][8] = {0};
    char buf[512]={0};
    int i = 0;

    for(i=0; i<TEST_N; i++)
    {
        sprintf(key[i], "%d", i);
        ab_hash_insert(hash, (void *)&key[i], strlen(key[i]), &key[i], NULL);
    }

    for(i=0; i<TEST_N; i++)
    {
        //ab_hash_get(hash, key[i], strlen(key[i]));
        CuAssertStrEquals(tc, ab_hash_get(hash, key[i], strlen(key[i]), 0), key[i]);
    }

    for(i=0; i<TEST_N; i++)
    {
        //ab_hash_delete(hash, key[i], strlen(key[i]));
        CuAssertStrEquals(tc, ab_hash_delete(hash, key[i], strlen(key[i]), 0), key[i]);
    }

    //ab_hash_foreach(hash, callback, buf);
    //printf("%s\n", buf);

    printf("%lld\n", ab_hash_num(hash));
    CuAssertTrue(tc, ab_hash_num(hash) == 0);

    ab_hash_destory(hash);
    printf("mem_leak:%llu\n", ab_mem_leak_check());
}


#define TEST_CNT 100000
#define THREADS  4
#define KEYLEN 8
ab_hash_t *g_hash = NULL;
char g_keybuf[THREADS][TEST_CNT][KEYLEN] = {0};
int g_keytimes[THREADS][TEST_CNT] = {0};

static void rand_keybuf()
{
    int tid = 0;

    srand(time(NULL));
    for (tid = 0; tid < THREADS; tid++)
    {
        int last = TEST_CNT-1;
        int i=0;
        char tmp[12] = {0};

        for(;last>0;last--)
        {
            int pos = rand()%last;
            strcpy(tmp, g_keybuf[tid][last]);
            strcpy(g_keybuf[tid][last], g_keybuf[tid][pos]);
            strcpy(g_keybuf[tid][pos], tmp);
        }
    }
}

void *test_func_put(void *arg)
{
    int i;
    int tid = (int)(size_t)arg;

    for(i=0; i<TEST_CNT; i++)
    {
        if(!ab_hash_insert(g_hash, g_keybuf[tid][i], KEYLEN, g_keybuf[tid][i], NULL))
        {
            printf("insert %s fail\n", g_keybuf[tid][i]);
        }
    }
    return NULL;
}

void *test_func_del(void *arg)
{
    int i;
    int tid = (int)(size_t)arg;
    int times = 0;
    int ret = 0;

    for(i=0; i<TEST_CNT; i++)
    {
        if (g_keytimes[tid][i] ++ > 1000)
        {
            printf("delete %s no found\n", g_keybuf[tid][i]);
            break;
        } 
        if (ab_hash_delete(g_hash, g_keybuf[tid][i], KEYLEN, 0) == NULL)
        {
            usleep(1);
            i--;
        }
    }
    return NULL;
}

//test concurrent-insert and concurrent-delete
void TEST_ab_hash_nonlock(CuTest* tc) 
{
    pthread_t id[20];
    int i = 0;
    int thnum = 0;

    //initralize test data
    g_hash = ab_hash_init(ab_attr_threadsafe, NULL, NULL, NULL);
    
    //2 put thread
    for (i=0; i<THREADS; i++)
    {
        pthread_create(&id[i], NULL, test_func_put, (void *)(long)i);
    }
    for (i=0; i<THREADS; i++)
    {
        pthread_join(id[i], NULL);
    }
    
    //2 del thread
    for (i=0; i<THREADS; i++)
    {
        pthread_create(&id[i], NULL, test_func_del, (void *)(long)i);
    }
    for (i=0; i<THREADS; i++)
    {
        pthread_join(id[i], NULL);
    }
    
    CuAssertTrue(tc, ab_hash_num(g_hash) == 0);
    
    ab_hash_destory(g_hash);
}

//test concurrent insert+delete
void TEST_ab_hash_nonlock_2(CuTest* tc) 
{
    pthread_t id[20];
    int i, thnum = 0;

    //initralize test data
    g_hash = ab_hash_init(ab_attr_threadsafe, NULL, NULL, NULL);

    //put and del threads concurrent execute 
    for (i=0; i<THREADS; i++)
    {
        pthread_create(&id[thnum++], NULL, test_func_put, (void *)(long)i);
        pthread_create(&id[thnum++], NULL, test_func_del, (void *)(long)i);
    }

    for (i=0; i<thnum; i++) pthread_join(id[i], NULL);

    printf("foreach\n");
    ab_hash_foreach(g_hash, callback, NULL);
    CuAssertTrue(tc, ab_hash_num(g_hash) == 0);
    
    ab_hash_destory(g_hash);
}

void TEST_ab_hash_may_thread(CuTest* tc) 
{
    int i = 0;

    for (i=0; i<10; i++)
    {
        TEST_ab_hash_nonlock_2(tc);
    }
}

CuSuite* CuGetSuite(void)
{
    CuSuite* suite = CuSuiteNew();

    //add tests
	SUITE_ADD_TEST(suite, TEST_ab_hash_insert);
    SUITE_ADD_TEST(suite, TEST_ab_hash_delete);
    SUITE_ADD_TEST(suite, TEST_ab_hash_more_keys);
    SUITE_ADD_TEST(suite, TEST_ab_hash_nonlock);
    SUITE_ADD_TEST(suite, TEST_ab_hash_nonlock_2);
    SUITE_ADD_TEST(suite, TEST_ab_hash_may_thread);
    
	return suite;
}   

int main(int argc, char** argv) 
{
    int i;
    int thid = 0;

    for (thid=0; thid<THREADS; thid++)
    {
        for(i=0; i<TEST_CNT; i++)
        {
            sprintf((char *)g_keybuf[thid][i], "%07d", i*THREADS+thid);
        }
    }
    //rand_keybuf(); 
    
    CuRunAllTests();
    return 0;
}

