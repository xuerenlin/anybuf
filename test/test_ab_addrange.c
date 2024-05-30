#include <stdlib.h>
#include "CuTest.h"
#include "ab_addrange.h"

void *test_func_thread(void *arg)
{
    int var1 = 0;
    int var2 = 0;
    int id1 = ab_addrange_id(&var1);
    int id2 = ab_addrange_id(&var2);
    printf("id = %u %u\n", id1, id2);
    return NULL;
}

int g_var = 0;
int *g_heap = 0;

void TEST_ab_addrange_id(CuTest* tc)
{
    pthread_t id[500];
    int i = 0;

    //The heap address must fail, cannot be found in the address range
    g_heap = (int *)malloc(sizeof(int));
    CuAssertTrue(tc, ab_addrange_id(g_heap) == ADDRANGE_MAX);

    //Global address, obtained id = 0
    printf("Global address: %d\n", ab_addrange_id(&g_var));
    CuAssertTrue(tc, ab_addrange_id(&g_var) == ADDRANGE_GLOBAL);

    //The main thread obtains the ID must be 1, because id = 0 is the global variable address range
    CuAssertTrue(tc, ab_addrange_id(&i) == 1);

    //create same thread to test 
    for (i=0; i<50; i++)
    {
        pthread_create(&id[i], NULL, test_func_thread, NULL);
    }
    for(i=0;i<50;++i)
    {
        pthread_join(id[i], NULL);
    }

    //create same thread again
    for (i=0; i<50; i++)
    {
        pthread_create(&id[i], NULL, test_func_thread, NULL);
    }
    for(i=0;i<50;++i)
    {
        pthread_join(id[i], NULL);
    }
}

CuSuite* CuGetSuite(void)
{
    CuSuite* suite = CuSuiteNew();

    //add tests
	SUITE_ADD_TEST(suite, TEST_ab_addrange_id);

	return suite;
}

int main(int argc, char** argv) 
{
    CuRunAllTests();
    return 0;
}
