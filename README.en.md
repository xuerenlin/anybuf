# anybuf

#### Introduction
Anybuf is a library implemented in C language, aimed at making C programming more convenient. The anybuf library mainly includes a universal lockless data structure that supports hash, skiplist, and queue data structures. It is also encapsulated as an ab_buf_t structure and supports a universal put/get/del interface. At the same time, in order to enable programming to also support JSON parsing and other commonly used components.


#### Instructions for use
1. Anybuf. c and Anybuf. h can be used directly, and all implementations are included in this file. Simply copy these two files into the direct project for use.
2. Alternatively,. so or. a files can be compiled:
   cd build; cmake ..; make

