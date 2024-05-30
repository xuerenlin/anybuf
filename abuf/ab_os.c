#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include "ab_global.h"
#include "ab_os.h"

static int  ab_os_gettid()
{
    return syscall(SYS_gettid);
}

static int  ab_os_getpid()
{
    return getpid();
}

static bool ab_os_thread_exsit(int os_tid)
{
    char proc_path[256] = {0};

    sprintf(proc_path, "/proc/%u/task/%u", ab_os_getpid(), os_tid);
    return (access(proc_path, 0) == 0);
}


static int g_thread_count = 0;
static int g_thread_os_tid[MAX_THREAD_NUM] = {0};

static int ab_search_thread_id()
{
    int i = 0;
    int os_cur_tid = ab_os_gettid();

    for (i=0; i<MAX_THREAD_NUM; i++)
    {
        int os_prev_id = g_thread_os_tid[i];
        if (!ab_os_thread_exsit(os_prev_id))    //this thread has exit
        {
            if(OS_CAS(&g_thread_os_tid[i], os_prev_id, os_cur_tid))
            {
                //fprintf(stdout, "threads %u-%u has exit, reuse it\n", i, os_prev_id);
                return i;
            }
        }
    }
    return -1;
}

int ab_get_thread_id() 
{
    static __thread int tid = -1;
    
    //new thread
    if (tid == -1) 
    {
        tid = OS_ATOMIC_ADD(&g_thread_count, 1);
        if (tid < MAX_THREAD_NUM) g_thread_os_tid[tid] = ab_os_gettid();
        else tid = ab_search_thread_id(tid);
    }

    if (tid < 0)
    {
        fprintf(stderr, "there is too many threads, max is %u\n", MAX_THREAD_NUM);
        exit(1);
    }

    return tid;
}

int ab_get_pagesize()
{
    return getpagesize();
}
