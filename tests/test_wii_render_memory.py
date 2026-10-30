"""Host ownership/alignment regression for the actual Wii renderer wrappers.
The SDK heap API is mocked; no game or graphics context is launched.
"""
from pathlib import Path
import re,os,subprocess,tempfile
root=Path(__file__).resolve().parents[1]
source=(root/'source/platform/wii/render_memory_wii.c').read_text(encoding='utf-8')
source=re.sub(r'^#include (?:<gccore.h>|<ogc/[^>]+>|"render_memory_wii.h")\s*$', '', source, flags=re.M)
stub=r'''
#define _POSIX_C_SOURCE 200112L
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
static unsigned char arena[12*1024*1024] __attribute__((aligned(32)));
static uintptr_t arena_low=(uintptr_t)arena+2*1024*1024,arena_high=(uintptr_t)arena+sizeof(arena);
static int force_heap_fail,force_real_fail,force_init_fail,real_frees,heap_frees;
static void *SYS_GetArena2Lo(void){return (void*)arena_low;}
static void *SYS_GetArena2Hi(void){return (void*)arena_high;}
static void SYS_SetArena2Lo(void *p){arena_low=(uintptr_t)p;}
#define _CPU_ISR_Disable(level) ((level)=0)
#define _CPU_ISR_Restore(level) ((void)(level))
typedef struct {void *start;size_t size,offset,active;} heap_cntrl;
typedef struct {unsigned free_size,used_size,free_blocks;} heap_iblock;
static struct {void *pointer;size_t bytes;int live;} blocks[2048];
static int block_count;
static int __lwp_heap_init(heap_cntrl *h,void *p,size_t bytes,size_t page){assert(page==32);if(force_init_fail)return 0;h->start=p;h->size=bytes;h->offset=h->active=0;return 1;}
static void *__lwp_heap_allocate(heap_cntrl *h,size_t bytes){
 if(force_heap_fail || bytes>h->size-h->offset)return NULL;
 void *p=(unsigned char*)h->start+h->offset;h->offset+=(bytes+31)&~(size_t)31;h->active+=bytes;
 assert(block_count<2048);blocks[block_count].pointer=p;blocks[block_count].bytes=bytes;blocks[block_count].live=1;++block_count;return p;
}
static void __lwp_heap_free(heap_cntrl *h,void *p){int found=0;for(int i=0;i<block_count;i++)if(blocks[i].pointer==p && blocks[i].live){blocks[i].live=0;h->active-=blocks[i].bytes;found=1;break;}assert(found);++heap_frees;}
static unsigned __lwp_heap_getinfo(heap_cntrl *h,heap_iblock *info){info->free_size=h->size-h->active;info->used_size=h->active;info->free_blocks=1;return 0;}
void Sys_Printf(char *format,...){(void)format;}
void *__real_memalign(size_t alignment,size_t bytes){void *p=NULL;if(force_real_fail)return NULL;if(alignment<sizeof(void*))alignment=sizeof(void*);return posix_memalign(&p,alignment,bytes)?NULL:p;}
void *__real_realloc(void *p,size_t n){if(force_real_fail && n)return NULL;return realloc(p,n);}
void __real_free(void *p){if(p)++real_frees;free(p);}
'''
main=r'''
int main(int argc,char **argv){
 uintptr_t original=arena_low;memset(arena,0x7a,2*1024*1024);
 if(argc>1){if(!strcmp(argv[1],"small"))arena_high=arena_low+256*1024;else force_init_fail=1;Wii_RenderMemoryInit();assert(!render_ready && arena_low==original);void *p=__wrap_memalign(64,1024);assert(p && !render_owns(p));__wrap_free(p);puts("PASS: unavailable/failed MEM2 leaves arena unchanged and uses libc");return 0;}
 void *cold=__wrap_memalign(32,256);assert(cold);
 Wii_RenderMemoryInit();assert(render_ready && render_low>=original && render_high==arena_low);
 assert(render_high-render_low<=8*1024*1024 && arena_high-arena_low>=2*1024*1024);
 __wrap_free(cold);assert(real_frees==1);
 for(size_t alignment=1;alignment<=4096;alignment*=2){
  unsigned char *p=__wrap_memalign(alignment,257);assert(p && render_owns(p) && !((uintptr_t)p&(alignment-1)));
  memset(p,0x5c,257);unsigned char *q=__wrap_realloc(p,1024);assert(q && render_owns(q) && !((uintptr_t)q&(alignment-1)));
  for(int i=0;i<257;i++)assert(q[i]==0x5c);
  p=__wrap_realloc(q,128);assert(p && render_owns(p));for(int i=0;i<128;i++)assert(p[i]==0x5c);__wrap_free(p);
 }
 unsigned char *p=__wrap_memalign(32,128);memset(p,0xa3,128);
 force_heap_fail=force_real_fail=1;assert(!__wrap_realloc(p,4096));for(int i=0;i<128;i++)assert(p[i]==0xa3);
 force_real_fail=0;void *fallback=__wrap_memalign(128,64);assert(fallback && !render_owns(fallback));__wrap_free(fallback);
 void *grown=__wrap_realloc(p,1024);assert(grown && !render_owns(grown));for(int i=0;i<128;i++)assert(((unsigned char*)grown)[i]==0xa3);__wrap_free(grown);
 force_heap_fail=0;p=__wrap_memalign(32,32);assert(!__wrap_realloc(p,0));
 p=malloc(128);assert(p && !render_owns(p));memset(p,1,128);grown=__wrap_realloc(p,256);assert(grown && !render_owns(grown));__wrap_free(grown);
 errno=0;assert(!__wrap_memalign(3,64) && errno==EINVAL);assert(!__wrap_memalign(32,SIZE_MAX));__wrap_free(NULL);
 assert(!render_heap.active);for(int i=0;i<2*1024*1024;i++)assert(arena[i]==0x7a);
 Wii_RenderMemoryReport("test complete");
 puts("PASS: aligned MEM2 ownership, cold/libc pointers, realloc preservation/failure, overflow, fallback, teardown and engine-hunk separation");
}
'''
with tempfile.TemporaryDirectory(prefix='nzp-render-memory-') as tmp:
 p=Path(tmp)/'test.c';exe=Path(tmp)/'test';p.write_text(stub+'\n'+source+'\n'+main,encoding='utf-8')
 subprocess.run([os.environ.get('CC','cc'),'-std=c99','-O2','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer',str(p),'-o',str(exe)],check=True)
 for args in ([],['small'],['initfail']):subprocess.run([str(exe),*args],check=True)
