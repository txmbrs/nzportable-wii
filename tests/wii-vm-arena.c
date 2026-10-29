#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define __wrap_malloc malloc
#define __wrap_free free
static void *vm_arena;
static int vm_arena_leased;
void *WiiOnline_ReserveVM(size_t bytes)
{
    if (bytes != 8u*1024*1024 || vm_arena_leased)
        return __wrap_malloc(bytes);
    if (!vm_arena) vm_arena=__wrap_malloc(bytes);
    if (vm_arena) vm_arena_leased=1;
    return vm_arena;
}
void WiiOnline_ReleaseVM(void *pointer)
{
    if (pointer && pointer==vm_arena) vm_arena_leased=0;
    else __wrap_free(pointer);
}

int main(void) {
 const size_t size=8u*1024*1024;
 unsigned char *menu=WiiOnline_ReserveVM(size);
 assert(menu); memset(menu,0xa5,size);
 void *concurrent=WiiOnline_ReserveVM(size);
 assert(concurrent && concurrent!=menu);
 WiiOnline_ReleaseVM(concurrent);
 WiiOnline_ReleaseVM(menu);
 void *noise[128];
 for(unsigned i=0;i<128;i++) noise[i]=malloc(4096+i*16);
 for(unsigned pass=0;pass<100;pass++) {
  unsigned char *game=WiiOnline_ReserveVM(size);
  assert(game==menu && game[0]==0xa5 && game[size-1]==0xa5);
  WiiOnline_ReleaseVM(game);
 }
 void *other=WiiOnline_ReserveVM(1024);
 assert(other && other!=menu); WiiOnline_ReleaseVM(other);
 for(unsigned i=0;i<128;i++) free(noise[i]);
 assert(!vm_arena_leased);
 free(vm_arena); vm_arena=NULL;
 puts("PASS: 100 VM handovers, concurrent lease, ordinary allocation ownership");
}
