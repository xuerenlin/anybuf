# ab_mem  
## 无锁内存池
### 1、空闲队列
内存池维护一个空闲队列，所有空闲的内存节点形成一个队列
ab_mem_t.free_head->item1->item2->...

### 2、内存块
当空闲队列中没有足够的节点时，申请一个内存块block，将block切分为多个空闲节点，并形成一个链表。然后挂接到空闲队列的free_head。同时，所有的block形成一个block链表，用于释放内存时使用：
ab_mem_t.block_head->block1->block2->...

### 3、无锁支持
#### 1）CAS操作
为了足够简单，在申请和释放时都只操作free_head，这样只在free_head上CAS操作即可:
申请：从free_head获取item
释放：将item归还到free_head

#### 2）ABA问题
由于申请和释放都是从free_head获取和释放，多线程产生ABA问题的概率非常大。所以这里在内存指针中加入ABA计数。
#define AB_ABA_PTR_MASK     0x0000FFFFFFFFFFFFULL
#define AB_ABA_CNT_MASK     0xFFFF000000000000ULL
#define AB_ABA_INCR         0x0001000000000000ULL
因为地址不会使用超过6个字节，所以高2个字节用于ABA计数，每次操作free_head是，从free_head中获取计数+1，用于新的free_head，保证CSA的准确性。

申请内存时，直接返回带ABA计数的指针，则其他数据结构也能够通过携带计数的地址解决ABA问题。


# ab_skiplist
     * Here's the sequence of events for a deletion of node n with
     * predecessor b and successor f, initially:
     *
     *        +------+       +------+      +------+
     *   ...  |   b  |------>|   n  |----->|   f  | ...
     *        +------+       +------+      +------+
     *
     * 1. CAS n's value field from non-null to null.
     *    From this point on, no public operations encountering
     *    the node consider this mapping to exist. However, other
     *    ongoing insertions and deletions might still modify
     *    n's next pointer.
     *
     * 2. CAS n's next pointer to point to a new marker node.
     *    From this point on, no other nodes can be appended to n.
     *    which avoids deletion errors in CAS-based linked lists.
     *
     *        +------+       +------+      +------+       +------+
     *   ...  |   b  |------>|   n  |----->|marker|------>|   f  | ...
     *        +------+       +------+      +------+       +------+
     *
     * 3. CAS b's next pointer over both n and its marker.
     *    From this point on, no new traversals will encounter n,
     *    and it can eventually be GCed.
     *        +------+                                    +------+
     *   ...  |   b  |----------------------------------->|   f  | ...
     *        +------+                                    +------+
     *

# ab_hash
  MySql中的LF_HASH原理
  https://zhuanlan.zhihu.com/p/452849776
  http://people.csail.mit.edu/shanir/publications/Split-Ordered_Lists.pdf

