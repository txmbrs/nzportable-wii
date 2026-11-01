/* GPL-2.0-or-later. Bounded ownership of accepted SCTP datagrams. */
#ifndef WII_DTLS_QUEUE_H
#define WII_DTLS_QUEUE_H
#include <stddef.h>
#include <string.h>
#define WII_DTLS_QUEUE_COUNT 16
#define WII_DTLS_QUEUE_PACKET 1200
#define WII_GAME_MTU 1000
typedef struct {
    struct {size_t length; unsigned char bytes[WII_DTLS_QUEUE_PACKET];} packet[WII_DTLS_QUEUE_COUNT];
    unsigned head,count;
} wii_dtls_queue_t;
/* 1 accepted; 0 queue full; -1 oversized. Acceptance transfers a copy. */
static inline int Wii_DTLSQueuePush(wii_dtls_queue_t *q,const void *data,size_t length)
{
    unsigned tail;
    if (!length || length>WII_DTLS_QUEUE_PACKET) return -1;
    if (q->count==WII_DTLS_QUEUE_COUNT) return 0;
    tail=(q->head+q->count)%WII_DTLS_QUEUE_COUNT;
    q->packet[tail].length=length;
    memcpy(q->packet[tail].bytes,data,length); ++q->count;
    return 1;
}
/* Writer: positive full length on success, 0 blocked, negative fatal.
 * Retain the same address, bytes and length across WANT_READ/WANT_WRITE. */
static inline int Wii_DTLSQueueDrain(wii_dtls_queue_t *q,void *ctx,
    int (*writer)(void *,const unsigned char *,size_t))
{
    while (q->count) {
        int result=writer(ctx,q->packet[q->head].bytes,q->packet[q->head].length);
        if (!result) return 0;
        if (result<0 || (size_t)result!=q->packet[q->head].length) return -1;
        q->head=(q->head+1)%WII_DTLS_QUEUE_COUNT; --q->count;
    }
    return 1;
}
#endif
