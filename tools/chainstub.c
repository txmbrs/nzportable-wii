/* GPL-2.0-or-later. Freestanding Wii RAM handoff; no IOS or storage calls. */
typedef unsigned int u32;
typedef unsigned char u8;
static void copy(u8 *to,const u8 *from,u32 n) { while(n--) *to++=*from++; }
static void sync_range(u32 address,u32 length)
{
    u32 p,end=address+length;
    for(p=address&~31u;p<end;p+=32) __asm__ volatile("dcbst 0,%0"::"r"(p):"memory");
    __asm__ volatile("sync":::"memory");
    for(p=address&~31u;p<end;p+=32) __asm__ volatile("icbi 0,%0"::"r"(p):"memory");
    __asm__ volatile("sync; isync":::"memory");
}
void relocate(const u32 *dol,const void *args)
{
    u32 i,j,entry=dol[56];
    for(i=0;i<18;i++) if(dol[36+i]) {
        copy((u8 *)dol[18+i],(const u8 *)dol+dol[i],dol[36+i]);
        sync_range(dol[18+i],dol[36+i]);
    }
    for(j=0;j<dol[55];j++) ((volatile u8 *)dol[54])[j]=0;
    if(dol[55]) sync_range(dol[54],dol[55]);
    if(*(u32 *)(entry+4)==0x5f617267) {
        copy((u8 *)(entry+8),(const u8 *)args,24);
        sync_range(entry+8,24);
    }
    ((void (*)(void))entry)();
    for(;;) __asm__ volatile("sync");
}
