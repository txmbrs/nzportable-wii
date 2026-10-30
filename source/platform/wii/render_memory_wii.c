/* Reserve otherwise unused MEM2 for aligned GPU/SDK buffers. GPL-2.0-or-later.
 * The engine hunk stays separate. Ordinary malloc remains in the libc heap. */
#include <gccore.h>
#include <ogc/lwp_heap.h>
#include <ogc/machine/processor.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <errno.h>
#include "render_memory_wii.h"

void Sys_Printf(char *format,...);
void *__real_memalign(size_t alignment,size_t bytes);
void *__real_realloc(void *pointer,size_t bytes);
void __real_free(void *pointer);
static heap_cntrl render_heap;
static uintptr_t render_low,render_high;
static int render_ready;
static unsigned render_fallbacks;
typedef struct {void *base;size_t size,alignment;} render_allocation_t;

void Wii_RenderMemoryInit(void)
{
    unsigned level;
    uintptr_t low,high,old_low;
    size_t bytes,keep=2u*1024*1024;
    if(render_ready)return;
    _CPU_ISR_Disable(level);
    old_low=(uintptr_t)SYS_GetArena2Lo();
    low=(old_low+31)&~(uintptr_t)31;
    high=(uintptr_t)SYS_GetArena2Hi();
    bytes=high>low && high-low>keep ? (high-low-keep)&~(size_t)31 : 0;
    if(bytes>8u*1024*1024)bytes=8u*1024*1024;
    if(bytes>=512u*1024 && __lwp_heap_init(&render_heap,(void *)low,bytes,32)) {
        render_low=low;render_high=low+bytes;
        SYS_SetArena2Lo((void *)render_high);
        render_ready=1;
    }
    _CPU_ISR_Restore(level);
    Sys_Printf("Wii renderer memory: MEM2 aligned pool %lu bytes; engine hunk unchanged\n",
        (unsigned long)(render_ready?render_high-render_low:0));
}
static int render_owns(const void *pointer)
{
    return render_ready && (uintptr_t)pointer>=render_low && (uintptr_t)pointer<render_high;
}
static void *render_allocate(size_t alignment,size_t bytes)
{
    void *base;
    uintptr_t aligned;
    unsigned level;
    render_allocation_t *record;
    if(alignment<32)alignment=32;
    if(alignment>SIZE_MAX-sizeof(*record) || bytes>SIZE_MAX-alignment-sizeof(*record))return NULL;
    _CPU_ISR_Disable(level);
    base=__lwp_heap_allocate(&render_heap,bytes+alignment+sizeof(*record));
    _CPU_ISR_Restore(level);
    if(!base)return NULL;
    aligned=((uintptr_t)base+sizeof(*record)+alignment-1)&~(uintptr_t)(alignment-1);
    record=(render_allocation_t *)aligned-1;
    record->base=base;record->size=bytes;record->alignment=alignment;
    return (void *)aligned;
}
void *__wrap_memalign(size_t alignment,size_t bytes)
{
    void *pointer=NULL;
    if(!alignment || (alignment&(alignment-1))) {errno=EINVAL;return NULL;}
    if(alignment>SIZE_MAX-sizeof(render_allocation_t) ||
        bytes>SIZE_MAX-alignment-sizeof(render_allocation_t)) {errno=ENOMEM;return NULL;}
    if(render_ready)pointer=render_allocate(alignment,bytes);
    if(!pointer) {
        unsigned level;
        if(render_ready) {_CPU_ISR_Disable(level);++render_fallbacks;_CPU_ISR_Restore(level);}
        pointer=__real_memalign(alignment,bytes);
    }
    return pointer;
}
void __wrap_free(void *pointer)
{
    if(render_owns(pointer)) {
        render_allocation_t *record=(render_allocation_t *)pointer-1;
        unsigned level;
        _CPU_ISR_Disable(level);
        __lwp_heap_free(&render_heap,record->base);
        _CPU_ISR_Restore(level);
    } else __real_free(pointer);
}
void *__wrap_realloc(void *pointer,size_t bytes)
{
    render_allocation_t *record;
    void *next;
    if(!render_owns(pointer))return __real_realloc(pointer,bytes);
    if(!bytes) {__wrap_free(pointer);return NULL;}
    record=(render_allocation_t *)pointer-1;
    next=__wrap_memalign(record->alignment,bytes);
    if(!next)return NULL;
    memcpy(next,pointer,bytes<record->size?bytes:record->size);
    __wrap_free(pointer);
    return next;
}
void Wii_RenderMemoryReport(const char *where)
{
    heap_iblock info;
    unsigned level,status=0,fallbacks;
    struct mallinfo libc=mallinfo();
    memset(&info,0,sizeof(info));
    _CPU_ISR_Disable(level);
    if(render_ready)status=__lwp_heap_getinfo(&render_heap,&info);
    fallbacks=render_fallbacks;
    _CPU_ISR_Restore(level);
    Sys_Printf("Wii renderer memory: %s; MEM2 free %u used %u check %u; libc free %lu; aligned fallbacks %u\n",
        where,info.free_size,info.used_size,status,(unsigned long)libc.fordblks,fallbacks);
}
