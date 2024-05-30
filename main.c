#include <stdio.h>
#include "anybuf.h"

typedef struct 
{
    int ki;
    char keys[20];
    char jsbin[256];
} kv_t;

void main()
{

    ab_buf_t buf = {0};
    ab_buf_init_fix(&buf, kv_t);
    ab_buf_attach_pk(&buf, kv_t, ki, type_num);

    kv_t kv1={1,"hello"};
    kv_t kv2={2,"world"};

    ab_buf_put(&buf, &kv1);
    ab_buf_put(&buf, &kv2);

    kv_t kvo={0};
    int k=2;
    ab_buf_del(&buf, &k, 0, &kvo, 1);
    printf("%s\n", kvo.keys);

    ab_buf_destory(&buf);
}
