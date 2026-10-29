/* GPL-2.0-or-later. No stdio, allocation, FAT or waiting in Record(). */
#include <gccore.h>
#include <ogc/machine/processor.h>
#include <stdio.h>
#include <string.h>
#include "wii_online_nettrace.h"

#define CAPACITY 128
typedef struct {
    unsigned event,stage,retries,operation;
    int result,previous,submission,payload;
} trace_event;
static trace_event ring[CAPACITY],last,last_entry;
static volatile unsigned head,tail,dropped;
static unsigned char seen[256][3];
static int values[256][3];
void WiiOnline_NetTraceLine(const char *message);

void WiiNetTrace_Record(unsigned event,unsigned stage,int result,int previous,
                        int submission,unsigned retries,int payload,unsigned operation)
{
    unsigned level,next;
    trace_event record={event,stage,retries,operation,result,previous,submission,payload};
    _CPU_ISR_Disable(level);
    next=(head+1)%CAPACITY;
    if(next==tail) { tail=(tail+1)%CAPACITY; ++dropped; }
    ring[head]=record;
    head=next;
    last=record;
    if(event==0) last_entry=record;
    _CPU_ISR_Restore(level);
}
void WiiNetTrace_Reset(void)
{
    unsigned level;
    _CPU_ISR_Disable(level);
    head=tail=dropped=0;
    memset(&last,0,sizeof(last)); memset(&last_entry,0,sizeof(last_entry));
    _CPU_ISR_Restore(level);
    memset(seen,0,sizeof(seen)); memset(values,0,sizeof(values));
}
static const char *stage_name(unsigned stage)
{
    static const char *names[]={"begin/open control","control open result","link-status result",
        "control close result","socket device open result","WC24 device open result",
        "WC24 retry","WC24 startup result","WC24 close result",
        "socket startup/IP retry","IP query result"};
    return stage<11 ? names[stage] : stage==255 ? "error cleanup" : "unknown";
}
static const char *operation_name(unsigned operation)
{
    static const char *names[]={"none","IOS_OpenAsync","IOS_CloseAsync","IOS_IoctlAsync",
                                "IOS_IoctlvFormatAsync","SYS_SetAlarm"};
    return operation<6 ? names[operation] : "unknown";
}
void WiiNetTrace_Drain(void)
{
    unsigned level;
    trace_event event;
    char message[256];
    for(;;) {
        _CPU_ISR_Disable(level);
        if(head==tail) { _CPU_ISR_Restore(level); break; }
        event=ring[tail]; tail=(tail+1)%CAPACITY;
        _CPU_ISR_Restore(level);
        unsigned stage=event.stage<256 ? event.stage : 255;
        if(event.event>2) continue;
        int value=event.event==1 ? (int)event.operation : event.result;
        /* Coalesce repeated retries, but preserve every new stage/result.
         * Current retries and dropped-event counts remain in Status(). */
        if(seen[stage][event.event] && values[stage][event.event]==value && !event.payload) continue;
        seen[stage][event.event]=1; values[stage][event.event]=value;
        if(event.event==0)
            snprintf(message,sizeof(message),"NET IOS: chain/callback entry stage %u (%s), result %d, prev %d, payload %d, retries %u",
                event.stage,stage_name(event.stage),event.result,event.previous,event.payload,event.retries);
        else if(event.event==1)
            snprintf(message,sizeof(message),"NET IOS: request %s; next stage %u (%s), prev %d, retries %u",
                operation_name(event.operation),event.stage,stage_name(event.stage),event.previous,event.retries);
        else
            snprintf(message,sizeof(message),"NET IOS: final stage %u (%s), result %d",event.stage,stage_name(event.stage),event.result);
        WiiOnline_NetTraceLine(message);
    }
}
void WiiNetTrace_Status(void)
{
    unsigned level,lost;
    trace_event current,callback;
    char message[256];
    _CPU_ISR_Disable(level);
    current=last; callback=last_entry; lost=dropped;
    _CPU_ISR_Restore(level);
    snprintf(message,sizeof(message),"NET IOS: latest stage %u (%s), event %u, operation %s, retries %u; last entry stage %u result %d; dropped %u",
        current.stage,stage_name(current.stage),current.event,operation_name(current.operation),current.retries,
        callback.stage,callback.result,lost);
    WiiOnline_NetTraceLine(message);
}
