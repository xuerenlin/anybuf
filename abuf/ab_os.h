#ifndef __AB_OS__
#define __AB_OS__

#define MAX_THREAD_NUM 100

#define CPU_BARRIER()       __asm__ __volatile__("mfence": : :"memory")
#define OS_CAS(p,o,n)       __sync_bool_compare_and_swap((p),(o),(n))
#define OS_ATOMIC_ADD(p,a)  __sync_fetch_and_add((p),(a));
#define OS_ATOMIC_SUB(p,a)  __sync_fetch_and_sub((p),(a));

int ab_get_thread_id();
int ab_get_pagesize();

#endif
