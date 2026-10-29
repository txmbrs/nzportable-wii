/* GPL-2.0-or-later. Validated MEM1 DOL loading, confined to volatile RAM. */
#include <gccore.h>
#include <ogc/system.h>
#include <ogc/machine/processor.h>
#include <wiiuse/wpad.h>
#include <sys/iosupport.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include "chainloader_wii.h"
#include "chainstub_bytes.h"

#define STAGE ((void *)0x92000000)
#define STUB ((void *)0x93000000)
#define ARGS ((void *)0x93200000)
static void (*launch_logger)(const char *);
void Wii_ChainloadSetLogger(void (*logger)(const char *)) { launch_logger=logger; }
static void progress(const char *message) { if(launch_logger) launch_logger(message); }
static int fail(char *error,unsigned int capacity,const char *message)
{ if(capacity) snprintf(error,capacity,"%s",message); return 0; }
static int range(unsigned int start,unsigned int length)
{ return start>=0x80003f00 && start<0x81800000 && length<=0x81800000-start; }
static int header(FILE *file,unsigned int *h,long *size,char *error,unsigned int capacity)
{
    unsigned int i,j;
    if(fseek(file,0,SEEK_END) || (*size=ftell(file))<256 || *size>0x1000000 ||
       fseek(file,0,SEEK_SET) || fread(h,1,256,file)!=256)
        return fail(error,capacity,"Cannot read DOL (or size exceeds 16 MiB).");
    if(h[55] && !range(h[54],h[55])) return fail(error,capacity,"Invalid DOL BSS.");
    for(i=0;i<18;i++) if(h[36+i]) {
        if(h[i]<256 || h[i]>(unsigned long)*size || h[36+i]>(unsigned long)*size-h[i] ||
           !range(h[18+i],h[36+i])) return fail(error,capacity,"Invalid DOL section.");
        for(j=0;j<i;j++) if(h[36+j] && h[18+i]<h[18+j]+h[36+j] && h[18+j]<h[18+i]+h[36+i])
            return fail(error,capacity,"Overlapping DOL sections.");
        if(h[55] && h[18+i]<h[54]+h[55] && h[54]<h[18+i]+h[36+i])
            return fail(error,capacity,"DOL BSS overlaps a section.");
    }
    for(i=0;i<7;i++) if(h[36+i]>=32 && h[56]>=h[18+i] && h[56]<=h[18+i]+h[36+i]-32 && !(h[56]&3)) return 1;
    return fail(error,capacity,"Invalid DOL entry point.");
}
int Wii_CheckDOL(const char *path,char *error,unsigned int capacity)
{
    unsigned int h[64]; long size;
    FILE *file=fopen(path,"rb");
    if(!file) return fail(error,capacity,"Online client missing. Install the Cooperation update.");
    int ok=header(file,h,&size,error,capacity);
    fclose(file); return ok;
}
static int load_dol(const char *path,void *buffer,unsigned int bytes,char *error,unsigned int capacity)
{
    unsigned int h[64],level; long size;
    struct __argv args;
    char command[1024];
    void *image;
    FILE *file;
    if(strlen(path)>=sizeof(command)-1 || !strstr(path,":/")) return fail(error,capacity,"Invalid module path.");
    strcpy(command,path);
    progress("COOP: opening and validating module\n");
    file=fopen(path,"rb");
    if(!file) return fail(error,capacity,"Cannot open module.");
    if(!header(file,h,&size,error,capacity)) { fclose(file); return 0; }
    if(buffer && ((unsigned int)buffer<0x90000000 || (unsigned int)buffer>=0x94000000 ||
       bytes>0x94000000-(unsigned int)buffer || (unsigned long)size>bytes)) {
        fclose(file);
        return fail(error,capacity,"Invalid or insufficient retired MEM2 buffer.");
    }
    image=buffer ? buffer : memalign(32,(size+31)&~31);
    if(!image) { fclose(file); return fail(error,capacity,"Insufficient memory for module."); }
    rewind(file);
    int ok=fread(image,1,size,file)==(size_t)size && !memcmp(image,h,256);
    fclose(file);
    if(!ok) { if(!buffer) free(image); return fail(error,capacity,"Cannot read complete module."); }
    progress("COOP: module read completely; header verified\n");
    memset(&args,0,sizeof(args));
    args.argvMagic=ARGV_MAGIC;
    args.commandLine=(char *)ARGS+256;
    args.length=strlen(command)+1;
    /* All allocation, storage and IOS shutdown precede fixed-address staging. */
    progress("COOP: shutting down controllers\n");
    WPAD_Shutdown();
    progress("COOP: controllers stopped; entering system shutdown and RAM handoff\n");
    SYS_ResetSystem(SYS_SHUTDOWN,0,0);
    _CPU_ISR_Disable(level);
    (void)level;
    memmove(STAGE,image,size);
    memcpy(STUB,chainstub_bytes,sizeof(chainstub_bytes));
    memcpy(ARGS,&args,sizeof(args));
    memcpy((char *)ARGS+256,command,args.length);
    DCFlushRange(STAGE,size);
    DCFlushRange(ARGS,256+args.length);
    DCFlushRange(STUB,sizeof(chainstub_bytes));
    ICInvalidateRange(STUB,sizeof(chainstub_bytes));
    ((void (*)(const void *,const void *))STUB)(STAGE,ARGS);
    for(;;) {}
}
int Wii_ChainloadDOL(const char *path,char *error,unsigned int capacity)
{ return load_dol(path,NULL,0,error,capacity); }
int Wii_ChainloadDOLWithBuffer(const char *path,void *buffer,unsigned int bytes,char *error,unsigned int capacity)
{
    if(!buffer || !bytes) return fail(error,capacity,"Retired MEM2 buffer unavailable.");
    return load_dol(path,buffer,bytes,error,capacity);
}
