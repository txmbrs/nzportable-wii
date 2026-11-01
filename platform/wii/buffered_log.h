/* GPL-2.0-or-later. Caller serializes append/take; this buffer performs no I/O. */
#ifndef WII_BUFFERED_LOG_H
#define WII_BUFFERED_LOG_H
#include <stddef.h>
#include <string.h>
#define WII_LOG_CAPACITY 65536u
typedef struct {char bytes[WII_LOG_CAPACITY];size_t head,size;unsigned long dropped;} wii_log_buffer_t;
static inline void Wii_LogAppend(wii_log_buffer_t *b,const char *data,size_t length)
{
    size_t tail,first,drop;
    if (length>WII_LOG_CAPACITY) {
        b->dropped+=length-WII_LOG_CAPACITY;
        data+=length-WII_LOG_CAPACITY; length=WII_LOG_CAPACITY;
    }
    drop=b->size+length>WII_LOG_CAPACITY?b->size+length-WII_LOG_CAPACITY:0;
    b->head=(b->head+drop)%WII_LOG_CAPACITY; b->size-=drop; b->dropped+=drop;
    tail=(b->head+b->size)%WII_LOG_CAPACITY;
    first=length<WII_LOG_CAPACITY-tail?length:WII_LOG_CAPACITY-tail;
    memcpy(b->bytes+tail,data,first); memcpy(b->bytes,data+first,length-first); b->size+=length;
}
static inline size_t Wii_LogTake(wii_log_buffer_t *b,char *out,size_t capacity)
{
    size_t n=b->size<capacity?b->size:capacity;
    size_t first=n<WII_LOG_CAPACITY-b->head?n:WII_LOG_CAPACITY-b->head;
    memcpy(out,b->bytes+b->head,first); memcpy(out+first,b->bytes,n-first);
    b->head=(b->head+n)%WII_LOG_CAPACITY; b->size-=n; return n;
}
#endif
