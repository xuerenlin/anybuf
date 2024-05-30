#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <sys/time.h>
#include <set>  

// test ab_buf
extern "C"
{
    #include "ab_buf.h"
    #include "CuTest.h"

    CuSuite* CuGetSuite(void){return NULL;}
}

typedef struct 
{
    int k;
    char name[20];
} contacts_t;

#define TEST_CNT 500000
#define TEST_DEL 1
int g_test_key[TEST_CNT] = {0};

static void rand_test_key()
{
    int last = TEST_CNT-1;
    int i=0;
    int tmp = 0;

    for (i=0; i<TEST_CNT; i++)
    {
        g_test_key[i] = i;
    }

    srand(time(NULL));

    for(;last>0;last--)
    {
        int pos = rand()%last;
        tmp = g_test_key[last];
        g_test_key[last] = g_test_key[pos];
        g_test_key[pos] = tmp;
    }
}

void ab_buf_perf_s()
{
    int i;
    contacts_t val = {0};
    ab_buf_t c_dir;

    ab_buf_init_fix(&c_dir, contacts_t);
    ab_buf_attach_pk(&c_dir, contacts_t, k, type_num);

    for (i=0; i<TEST_CNT; i++)
    {
        val.k = g_test_key[i];
        ab_buf_put(&c_dir, &val);
    }
    if(TEST_DEL)
    {
        for (i=0; i<TEST_CNT; i++)
        {
            ab_buf_del(&c_dir, &i, 0, NULL, 1);
        }
    }
    ab_buf_destory(&c_dir);
}

struct cmp
{
	bool operator ()(const contacts_t &a, const contacts_t &b) const
	{
		return a.k < b.k;
	}
};

void std_set_perf()
{
    int i;
    contacts_t val = {0};
    std::set<contacts_t, cmp> setBuf;
    
    for (i=0; i<TEST_CNT; i++)
    {
        val.k = g_test_key[i];
        setBuf.insert(val);
    }
    if(TEST_DEL)
    {
        for (i=0; i<TEST_CNT; i++)
        {
            val.k = g_test_key[i];
            setBuf.erase(val);
        }
    }
}

int main(int argc, char** argv) 
{
    rand_test_key();

    TEST_TIME(ab_buf_perf_s);
    TEST_TIME(std_set_perf);
    
    return 0;
}
