#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>

#include "CuTest.h"    
#include "ab_error.h"
#include "ab_skiplist.h"
#include "ab_mem.h"

static int compare_value(void *key1, void *key2, void *param)
{
    return strcmp((char *)key1, (char *)key2);
}

static int print_value(void *k, void *v, void *param)
{
    printf("%s ", (char *)k);
    return 0;
}

#define TEST_CNT  100000

unsigned char keybuf1[TEST_CNT][12] = {0};

void TEST_ab_skiplist_insert(CuTest* tc) 
{
    int i;
    int keylen = 0;
    
    ab_skiplist_t *list = ab_skiplist_init(0, compare_value, print_value, NULL);

    for(i=0; i<TEST_CNT; i++)
    {
        keylen = sprintf((char *)keybuf1[i], "k%04d", i);
        ab_ret_t ret = ab_skiplist_insert(list, keybuf1[i], keybuf1[i], NULL);
        CuAssertTrue(tc, ret==0);
    }

    for(i=TEST_CNT-1; i>=0; i--)
    {
        void *value = ab_skiplist_delete(list, keybuf1[i], 0);
        CuAssertTrue(tc, value==keybuf1[i]);
    }

    //ab_skiplist_print(list);
    CuAssertTrue(tc, ab_skiplist_is_empty(list));

    ab_skiplist_destory(list);
}

void TEST_ab_skiplist_insert_dupkey(CuTest* tc) 
{
    int i;
    int keylen = 0;
    ab_skiplist_t *list = ab_skiplist_init(ab_attr_duplicate, compare_value, print_value, NULL);

    for(i=0; i<TEST_CNT; i++)
    {
        keylen = sprintf((char *)keybuf1[i], "k%04d", i%(TEST_CNT/10));
        ab_skiplist_insert(list, keybuf1[i], keybuf1[i], NULL);
    }
    //ab_skiplist_print(list);
    
    for(i=TEST_CNT-1; i>=0; i--)
    {
        ab_skiplist_delete(list, keybuf1[i], 0);
    }

    //ab_skiplist_print(list);
    CuAssertTrue(tc, ab_skiplist_is_empty(list));

    ab_skiplist_destory(list);
}


#define THREADS  4
ab_skiplist_t *g_list = NULL;
char g_keybuf[THREADS][TEST_CNT][12] = {0};

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
        ab_skiplist_insert(g_list, g_keybuf[tid][i], g_keybuf[tid][i], NULL);
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
        if (ab_skiplist_delete(g_list, g_keybuf[tid][i], 0) == NULL)
        {
            //printf("delete %s no found, wait the put thread and retry\n", g_keybuf[tid][i]);
            usleep(1);
            i--;

            //if (times++ > 10000)
            //{
            //    i++;
            //    printf("get %s fail ret=%d, times:%d\n", g_keybuf[tid][i], ret, times);
            //    times=0;
            //}
        }
    }
    return NULL;
}

//test concurrent-insert and concurrent-delete
void TEST_ab_skiplist_nonlock(CuTest* tc) 
{
    pthread_t id[20];
    int i = 0;
    int thnum = 0;

    //initralize test data
    g_list = ab_skiplist_init(ab_attr_threadsafe, compare_value, print_value, NULL);
    
    
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
    
    //ab_skiplist_print(g_list);
    CuAssertTrue(tc, ab_skiplist_is_empty(g_list));
    
    ab_skiplist_destory(g_list);
}

//test concurrent insert+delete
void TEST_ab_skiplist_nonlock_2(CuTest* tc) 
{
    pthread_t id[20];
    int i, thnum = 0;

    //initralize test data
    g_list = ab_skiplist_init(ab_attr_threadsafe, compare_value, print_value, NULL);

    //put and del threads concurrent execute 
    for (i=0; i<THREADS; i++)
    {
        pthread_create(&id[thnum++], NULL, test_func_put, (void *)(long)i);
        pthread_create(&id[thnum++], NULL, test_func_del, (void *)(long)i);
    }

    for (i=0; i<thnum; i++) pthread_join(id[i], NULL);

    //ab_skiplist_print(g_list);
    CuAssertTrue(tc, ab_skiplist_is_empty(g_list));

    ab_skiplist_destory(g_list);
}

//test concurrent insert+delete
void ab_test_nonlock_duplicate(CuTest* tc) 
{
    pthread_t id[20];
    int i, thnum = 0;
    int thid = 0;

    for (thid=0; thid<THREADS; thid++)
    {
        for(i=0; i<TEST_CNT; i++)
        {
            sprintf((char *)g_keybuf[thid][i], "k%04d", (i*THREADS+thid)/100);  //100 duplicate key
        }
    }

    //initralize test data
    g_list = ab_skiplist_init(ab_attr_duplicate|ab_attr_threadsafe, compare_value, print_value, NULL);

    //put and del threads concurrent execute 
    for (i=0; i<THREADS; i++)
    {
        pthread_create(&id[thnum++], NULL, test_func_put, (void *)(long)i);
        pthread_create(&id[thnum++], NULL, test_func_del, (void *)(long)i);
    }

    for (i=0; i<thnum; i++) pthread_join(id[i], NULL);

    //ab_skiplist_print(g_list);
    CuAssertTrue(tc, ab_skiplist_is_empty(g_list));

    ab_skiplist_destory(g_list);
}

void TEST_ab_skiplist_nonlock_duplicate(CuTest* tc) 
{
    int i=0; 
    for (i=0; i<1; i++)
    {
        ab_test_nonlock_duplicate(tc);
    }
}

CuSuite* CuGetSuite(void)
{
    CuSuite* suite = CuSuiteNew();

    //add tests
	SUITE_ADD_TEST(suite, TEST_ab_skiplist_insert);
    SUITE_ADD_TEST(suite, TEST_ab_skiplist_insert_dupkey);
    SUITE_ADD_TEST(suite, TEST_ab_skiplist_nonlock);
    SUITE_ADD_TEST(suite, TEST_ab_skiplist_nonlock_2);
    SUITE_ADD_TEST(suite, TEST_ab_skiplist_nonlock_duplicate);

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
            sprintf((char *)g_keybuf[thid][i], "k%04d", i*THREADS+thid);
        }
    }
    //rand_keybuf();

    CuRunAllTests();
    return 0;
}


