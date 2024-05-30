# anybuf

#### 介绍
anybuf是一款用C语言实现的库，目的是使得C语言编程更为方便。anybuf库主要包含了一个通用的无锁数据结构，支持hash、skiplist、queue数据结构，同时封装为一个ab_buf_t结构，支持通用的put/get/del接口。同时为了让编程还支持json解析，以及其他一些常用的组件。


#### 使用说明
1.  可以直接使用anybuf.c和anybuf.h，所有实现都包含进这个文件中，直接拷贝这两个文件到直接的项目中即可使用。
2.  或者可以编译.so或.a文件: 
    cd build; cmake ..; make
