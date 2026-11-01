/* Native Wii bootstrap and MEM2 allocator for the FTE online client.
 * Port additions licensed under GPL-2.0-or-later. */
#include <gccore.h>
#include <ogc/lwp_heap.h>
#include <ogc/machine/processor.h>
#include <ogc/ios.h>
#include <ogc/lwp_watchdog.h>
#include <ogc/usbmouse.h>
#include <wiikeyboard/keyboard.h>
#include <fat.h>
#include <network.h>
#include <wiiuse/wpad.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include "wii_online_nettrace.h"
#include "../../../../platform/wii/buffered_log.h"

static heap_cntrl online_heap;
static uintptr_t heap_low, heap_high;
static int heap_ready;
int WiiOnline_InitRandom(void);
static char online_log_path[1200];
static FILE *online_log_file;
static wii_log_buffer_t runtime_log;
static int log_runtime;
static void flush_online_log(void);
static int log_warning;
static GXRModeObj *boot_mode;
static void *boot_xfb;
static char shared_base[1024];
static int home_exit;
void WiiOnline_Fatal(const char *text);
const char *WiiOnline_BaseDir(void) { return shared_base; }
void WiiOnline_HomeExit(void) { home_exit=1; }
int Wii_ChainloadDOL(const char *,char *,unsigned int);
void WiiOnline_Return(void)
{
    char path[1200],error[256];
    flush_online_log();
    if(online_log_file) { fflush(online_log_file); fclose(online_log_file); online_log_file=NULL; }
    if(home_exit) return;
    snprintf(path,sizeof(path),"%sboot.dol",shared_base);
    if(!Wii_ChainloadDOL(path,error,sizeof(error))) WiiOnline_Fatal(error);
}

void *__real_malloc(size_t bytes);
void *__real_calloc(size_t count, size_t bytes);
void *__real_realloc(void *ptr, size_t bytes);
void *__real_memalign(size_t alignment, size_t bytes);
void __real_free(void *ptr);
typedef struct { void *base; size_t size; } allocation_t;
#define LARGE_ALLOCATION (256*1024)
typedef struct { void *pointer; size_t bytes; uintptr_t caller; } large_allocation;
static large_allocation large_allocations[64];
static unsigned int untracked_large;
static void track_large(void *pointer,size_t bytes,uintptr_t caller)
{
    unsigned int level,i,empty=64;
    if(!pointer || bytes<LARGE_ALLOCATION) return;
    _CPU_ISR_Disable(level);
    for(i=0;i<64;i++) {
        if(large_allocations[i].pointer==pointer) { empty=i; break; }
        if(!large_allocations[i].pointer && empty==64) empty=i;
    }
    if(empty<64) {
        large_allocations[empty].pointer=pointer;
        large_allocations[empty].bytes=bytes;
        large_allocations[empty].caller=caller;
    } else ++untracked_large;
    _CPU_ISR_Restore(level);
}
static void untrack_large(void *pointer)
{
    unsigned int level,i;
    _CPU_ISR_Disable(level);
    for(i=0;i<64;i++) if(large_allocations[i].pointer==pointer) {
        large_allocations[i].pointer=NULL; break;
    }
    _CPU_ISR_Restore(level);
}

static int is_mem2(const void *ptr)
{ return heap_ready && (uintptr_t)ptr >= heap_low && (uintptr_t)ptr < heap_high; }
static void *allocate_mem2(size_t alignment, size_t bytes)
{
    void *base;
    unsigned int level;
    uintptr_t aligned;
    allocation_t *info;
    if (alignment < 32) alignment = 32;
    if ((alignment & (alignment-1)) || alignment > UINT32_MAX-sizeof(*info) ||
        bytes > UINT32_MAX-alignment-sizeof(*info)) { errno=ENOMEM; return NULL; }
    /* Bluetooth may allocate with interrupts already masked. A sleeping mutex
     * can deadlock if its owner was interrupted; preserve the caller's mask. */
    _CPU_ISR_Disable(level);
    base = __lwp_heap_allocate(&online_heap, bytes+alignment+sizeof(*info));
    _CPU_ISR_Restore(level);
    if (!base) return NULL;
    aligned = ((uintptr_t)base+sizeof(*info)+alignment-1) & ~(uintptr_t)(alignment-1);
    info = (allocation_t *)aligned-1;
    info->base = base;
    info->size = bytes;
    return (void *)aligned;
}
void *__wrap_malloc(size_t bytes)
{
    void *ptr = heap_ready ? allocate_mem2(32, bytes) : NULL;
    if(!ptr) ptr=__real_malloc(bytes);
    track_large(ptr,bytes,(uintptr_t)__builtin_return_address(0));
    return ptr;
}
void __wrap_free(void *ptr);
/* Menu and game VMs are exclusive on Wii. Keep their 8 MiB arena intact
 * across handover/reconnect, so texture allocations cannot fragment it. */
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

void *__wrap_memalign(size_t alignment, size_t bytes)
{
    void *ptr;
    if (!alignment || (alignment & (alignment-1))) { errno=EINVAL; return NULL; }
    ptr = heap_ready ? allocate_mem2(alignment, bytes) : NULL;
    if(!ptr) ptr=__real_memalign(alignment, bytes);
    track_large(ptr,bytes,(uintptr_t)__builtin_return_address(0));
    return ptr;
}
void *__wrap_calloc(size_t count, size_t bytes)
{
    void *ptr;
    if (bytes && count > SIZE_MAX/bytes) { errno=ENOMEM; return NULL; }
    ptr = __wrap_malloc(count*bytes);
    if (ptr) memset(ptr, 0, count*bytes);
    track_large(ptr,count*bytes,(uintptr_t)__builtin_return_address(0));
    return ptr;
}
void __wrap_free(void *ptr)
{
    if (!ptr) return;
    untrack_large(ptr);
    if (is_mem2(ptr)) {
        allocation_t *info = (allocation_t *)ptr-1;
        unsigned int level;
        _CPU_ISR_Disable(level);
        __lwp_heap_free(&online_heap, info->base);
        _CPU_ISR_Restore(level);
    } else __real_free(ptr);
}
void *__wrap_realloc(void *ptr, size_t bytes)
{
    void *next;
    allocation_t *info;
    if (!ptr) return __wrap_malloc(bytes);
    if (!bytes) { __wrap_free(ptr); return NULL; }
    if (!is_mem2(ptr)) {
        next=__real_realloc(ptr,bytes);
        if(next) {
            untrack_large(ptr);
            track_large(next,bytes,(uintptr_t)__builtin_return_address(0));
        }
        return next;
    }
    info = (allocation_t *)ptr-1;
    next = __wrap_malloc(bytes);
    if (!next) return NULL;
    memcpy(next, ptr, bytes < info->size ? bytes : info->size);
    __wrap_free(ptr);
    track_large(next,bytes,(uintptr_t)__builtin_return_address(0));
    return next;
}

static int open_online_log(void)
{
    if(!online_log_file && online_log_path[0])online_log_file=fopen(online_log_path,"ab");
    if(!online_log_file && !log_warning++)printf("WARNING: cannot open online startup log.\n");
    return online_log_file!=NULL;
}
static void flush_online_log(void)
{
    char chunk[1024];size_t n;unsigned level;
    if(!open_online_log())return;
    if(runtime_log.dropped){fprintf(online_log_file,"\n[log buffer dropped %lu oldest bytes]\n",runtime_log.dropped);runtime_log.dropped=0;}
    do {
        _CPU_ISR_Disable(level);n=Wii_LogTake(&runtime_log,chunk,sizeof(chunk));_CPU_ISR_Restore(level);
        if(n && fwrite(chunk,1,n,online_log_file)!=n){if(!log_warning++)printf("WARNING: online log write failed.\n");break;}
    }while(n);
    if(fflush(online_log_file) && !log_warning++)printf("WARNING: online log flush failed.\n");
}
void WiiOnline_BeginRuntimeLog(void);
void WiiOnline_BeginRuntimeLog(void)
{
    flush_online_log();log_runtime=1;
}
void WiiOnline_Log(const char *text)
{
    unsigned level;
    if(log_runtime){_CPU_ISR_Disable(level);Wii_LogAppend(&runtime_log,text,strlen(text));_CPU_ISR_Restore(level);return;}
    if(open_online_log()){fputs(text,online_log_file);fflush(online_log_file);}
}
void WiiOnline_Fatal(const char *text)
{
    log_runtime=0;flush_online_log();
    if (boot_xfb) {
        VIDEO_Configure(boot_mode);
        VIDEO_SetNextFramebuffer(boot_xfb);
        console_init(boot_xfb, 20,20,boot_mode->fbWidth-40,boot_mode->xfbHeight-40,boot_mode->fbWidth*2);
        VIDEO_SetBlack(FALSE);
        VIDEO_Flush();
        VIDEO_WaitVSync();
    }
    WiiOnline_Log(text);
    flush_online_log();
    if(online_log_file) { fflush(online_log_file); fclose(online_log_file); online_log_file=NULL; }
    printf("\nNZPortable Wii Online (development)\n%s\nHOME / START: exit\n", text);
    for (;;) {
        WPAD_ScanPads(); PAD_ScanPads();
        if ((WPAD_ButtonsDown(0)&WPAD_BUTTON_HOME) || (PAD_ButtonsDown(0)&PAD_BUTTON_START)) break;
        VIDEO_WaitVSync();
    }
    exit(1);
}
static int data_here(void)
{ struct stat st; return !stat("compat.pk3", &st) && !stat("../nzp",&st) && S_ISDIR(st.st_mode); }
static void startup_step(const char *message)
{
    printf("%s\n",message); fflush(stdout);
    WiiOnline_Log(message); WiiOnline_Log("\n");
}
void WiiOnline_RandomProgress(const char *message) { startup_step(message); }
void WiiOnline_NetTraceLine(const char *message) { startup_step(message); }
/* Main-thread diagnostics only. Allocator callbacks merely update RAM records. */
void WiiOnline_MemoryReport(const char *where)
{
    large_allocation snapshot[64];
    heap_iblock info;
    unsigned int level,status,largest=0,i,lost,blocks=0;
    uintptr_t current;
    char message[224];
    if(!heap_ready) return;
    _CPU_ISR_Disable(level);
    status=__lwp_heap_getinfo(&online_heap,&info);
    current=(uintptr_t)online_heap.start;
    while(!status && current>=heap_low && current<=heap_high-HEAP_OVERHEAD && blocks++<200000) {
        heap_block *block=(heap_block *)current;
        unsigned int flag=block->front_flag,size=flag&~HEAP_BLOCK_USED;
        if(flag==HEAP_DUMMY_FLAG) break;
        if(!size || size>heap_high-current) { status=3; break; }
        if(!(flag&HEAP_BLOCK_USED) && size>largest) largest=size;
        current+=size;
    }
    memcpy(snapshot,large_allocations,sizeof(snapshot)); lost=untracked_large;
    _CPU_ISR_Restore(level);
    struct mallinfo libc=mallinfo();
    snprintf(message,sizeof(message),"MEM: %s; MEM2 %08lx..%08lx (%lu bytes), heap check %u",where,
        (unsigned long)heap_low,(unsigned long)heap_high,(unsigned long)(heap_high-heap_low),status);
    startup_step(message);
    snprintf(message,sizeof(message),"MEM: free %u, used %u, largest free block %u, free blocks %u; libc free %lu",info.free_size,info.used_size,
        largest,info.free_blocks,(unsigned long)libc.fordblks);
    startup_step(message);
    for(i=0;i<64;i++) if(snapshot[i].pointer) {
        snprintf(message,sizeof(message),"MEM: live allocation %lu bytes at %p, caller %08lx",(unsigned long)snapshot[i].bytes,
            snapshot[i].pointer,(unsigned long)snapshot[i].caller);
        startup_step(message);
    }
    if(lost) { snprintf(message,sizeof(message),"MEM: large allocation records omitted %u",lost); startup_step(message); }
}
/* Keep query state alive if an IOS call fails to return before the UI timeout.
 * Never kill a thread in an IOS/FAT operation or join it on the timeout path. */
static struct {
    volatile int done;
    int result;
    char ip[16];
} network_query;
static void *query_network_address(void *unused)
{
    unsigned int level;
    (void)unused;
    /* net_init_async has completed: if_config's net_init fast path is now
     * non-waiting. Keep SDK socket setup, skip unused mask/gateway queries. */
    int result=if_config(network_query.ip,NULL,NULL,true,1);
    _CPU_ISR_Disable(level);
    network_query.result=result;
    network_query.done=1;
    _CPU_ISR_Restore(level);
    return NULL;
}
static unsigned int elapsed_ms(u64 start)
{ return (unsigned int)ticks_to_millisecs(gettime()-start); }
static void init_network(void)
{
    char status[512];
    int result = -1;
    unsigned int next_message = 0;
    u64 start;
    lwp_t thread;
    int attempt = 0;

    startup_step("NET: starting IOS network asynchronously (45-second limit)");
    start = gettime();

    while (elapsed_ms(start) < 45000) {
        attempt++;
        WiiNetTrace_Reset();
        result = net_init_async(NULL, NULL);
        snprintf(status, sizeof(status), "NET: net_init_async (attempt %d) returned %d", attempt, result);
        startup_step(status);

        if (result == 0 || result == -EBUSY) {
            while ((result = net_get_status()) == -EBUSY) {
                WiiNetTrace_Drain();
                unsigned int ms = elapsed_ms(start);
                if (ms >= 45000) break;
                if (ms >= next_message) {
                    snprintf(status, sizeof(status), "NET: waiting for IOS/DHCP (%u / 45s), status %d", ms / 1000, result);
                    startup_step(status);
                    WiiNetTrace_Status();
                    next_message = ms + 5000;
                }
                usleep(100000);
            }
        }

        WiiNetTrace_Drain();
        WiiNetTrace_Status();

        if (result >= 0) {
            snprintf(status, sizeof(status), "NET: IOS initialization succeeded on attempt %d, status %d", attempt, result);
            startup_step(status);
            break;
        }

        if (elapsed_ms(start) >= 45000) break;

        snprintf(status, sizeof(status), "NET: attempt %d failed (code %d), retrying link in 2s...", attempt, result);
        startup_step(status);

        /* Only tear down a completed failure. net_deinit waits without its
         * own timeout if an async init is still in progress. */
        if (net_get_status()==-EBUSY) break;
        net_deinit();
        unsigned spent=elapsed_ms(start);
        if (spent>=43000) break;
        usleep(2000000);
    }

    if (result < 0) {
        snprintf(status, sizeof(status),
            "IOS/DHCP initialization failed: code %d (after %d attempts).\n"
            "Network initialization did not complete.\n"
            "Check Internet -> Connection Test in Wii System Settings.\n"
            "Saved Wii network settings were not changed.",
            result, attempt);
        WiiOnline_Fatal(status);
    }
    startup_step("NET: reading local IPv4 address and preparing socket API (15-second limit)");
    memset(&network_query,0,sizeof(network_query));
    result=LWP_CreateThread(&thread,query_network_address,NULL,NULL,32768,80);
    if(result<0) WiiOnline_Fatal("Cannot create network address query thread.");
    start=gettime(); next_message=5000;
    while(!network_query.done) {
        unsigned int ms=elapsed_ms(start);
        if(ms>=15000) WiiOnline_Fatal("IOS address/socket query timed out after 15 seconds. See NET lines in the startup log.");
        if(ms>=next_message) {
            snprintf(status,sizeof(status),"NET: waiting for address/socket query (%u / 15 seconds)",ms/1000);
            startup_step(status); next_message=ms+5000;
        }
        usleep(100000);
    }
    LWP_JoinThread(thread,NULL);
    snprintf(status,sizeof(status),"NET: address query returned %d; IPv4 %s",network_query.result,network_query.ip);
    startup_step(status);
    if(network_query.result<0 || !network_query.ip[0] || !strcmp(network_query.ip,"0.0.0.0"))
        WiiOnline_Fatal("IOS network did not provide a usable IPv4 address. See the NET result code in the startup log.");
    startup_step("BOOT: IOS network ready");
}
static void init_platform(int argc, char **argv)
{
    char path[1024], original[1024];
    uintptr_t low, high;
    int found = 0, result, ios_version;
    VIDEO_Init();
    boot_mode = VIDEO_GetPreferredMode(NULL);
    boot_xfb = SYS_AllocateFramebuffer(boot_mode);
    if (!boot_xfb) exit(1);
    boot_xfb = MEM_K0_TO_K1(boot_xfb);
    VIDEO_Configure(boot_mode); VIDEO_SetNextFramebuffer(boot_xfb);
    console_init(boot_xfb,20,20,boot_mode->fbWidth-40,boot_mode->xfbHeight-40,boot_mode->fbWidth*2);
    VIDEO_SetBlack(FALSE); VIDEO_Flush(); VIDEO_WaitVSync();
    printf("NZPortable Wii Online - startup r18ba\nVIDEO ready.\n");
    fflush(stdout);
    /* The old PPC image is gone, but its IOS USB/Bluetooth device state need
     * not be. Reload the currently running installed IOS before opening FAT or
     * any device in this image. This neither installs nor patches an IOS. */
    ios_version=IOS_GetVersion();
    printf("Restarting current IOS %d for USB/Bluetooth...\n",ios_version); fflush(stdout);
    if(ios_version<=0 || ios_version>255) WiiOnline_Fatal("Cannot identify current IOS.");
    result=IOS_ReloadIOS(ios_version);
    if(result<0) WiiOnline_Fatal("Cannot reload current IOS for clean device startup.");
    startup_step("IOS ready; mounting storage...");
    if (!fatInitDefault()) WiiOnline_Fatal("Cannot mount SD/USB.");
    /* Mount and select the installation before Bluetooth/network setup. This
     * lets us persist diagnostics even if a later device reinitialization stalls. */
    if (!getcwd(original, sizeof(original))) strcpy(original, "/");
    if (argc && argv && argv[0] && strstr(argv[0], ":/") && strlen(argv[0]) < sizeof(path)) {
        strcpy(path, argv[0]);
        char *slash = strrchr(path, '/');
        if (slash) { *slash=0; found = !chdir(path) && data_here(); }
    }
    if (!found) found = !chdir(original) && data_here();
    if (!found) found = !chdir("sd:/apps/nzportable/online") && data_here();
    if (!found) found = !chdir("usb:/apps/nzportable/online") && data_here();
    if (!found) WiiOnline_Fatal("Keep online/client.dol and online/compat.pk3 beside the existing nzp game directory.");
    if(!getcwd(shared_base,sizeof(shared_base))) WiiOnline_Fatal("Cannot resolve installation directory.");
    char *last=strrchr(shared_base,'/');
    if(!last) WiiOnline_Fatal("Invalid installation directory.");
    last[1]=0;
    if(!getcwd(path,sizeof(path))) WiiOnline_Fatal("Cannot resolve online log directory.");
    snprintf(online_log_path,sizeof(online_log_path),"%s/wii-online-startup.txt",path);
    FILE *log=fopen(online_log_path,"wb");
    if(log) fclose(log);
    else { printf("WARNING: cannot create online startup log.\n"); log_warning=1; }
    startup_step("BOOT: FTE r18ba; storage ready; initializing MEM2 before controllers");
    low = ((uintptr_t)SYS_GetArena2Lo()+31)&~(uintptr_t)31;
    high = (uintptr_t)SYS_GetArena2Hi()&~(uintptr_t)31;
    if (high <= low+1024*1024) WiiOnline_Fatal("MEM2 unavailable.");
    high -= 1024*1024; /* Leave a separate arena reservation for other IOS/libogc users. */
    if(high>0x931f0000) high=0x931f0000; /* Keep handoff argv and stack outside the heap. */
    if (high<=low+1024*1024 || !__lwp_heap_init(&online_heap,(void *)low,high-low,32))
        WiiOnline_Fatal("Cannot initialize MEM2 allocator.");
    SYS_SetArena2Lo((void *)high);
    heap_low = low; heap_high = high; heap_ready=1;
    WiiOnline_MemoryReport("allocator initialized");
    startup_step("BOOT: MEM2 ready; PAD_Init");
    PAD_Init();
    startup_step("BOOT: PAD ready; WPAD_Init (Bluetooth)");
    result=WPAD_Init();
    char status[96];
    snprintf(status,sizeof(status),"BOOT: WPAD_Init returned %d",result);
    startup_step(status);
    if(result<0) WiiOnline_Fatal("WPAD_Init failed; see the return code in the startup log.");
    startup_step("BOOT: WPAD_SetIdleTimeout");
    WPAD_SetIdleTimeout(3600);
    startup_step("BOOT: WPAD_SetDataFormat");
    result=WPAD_SetDataFormat(0, WPAD_FMT_BTNS_ACC_IR);
    snprintf(status,sizeof(status),"BOOT: WPAD_SetDataFormat returned %d; WPAD_SetVRes",result);
    startup_step(status);
    WPAD_SetVRes(0, 640, 480);
    /* SDL's OGC event pump reads both queues even when no USB keyboard or
     * mouse is attached. SDL2main used to create these; our native entry must
     * do so before any SDL/FTE event pump can run. */
    startup_step("BOOT: SDL USB input queues; MOUSE_Init");
    result=MOUSE_Init();
    snprintf(status,sizeof(status),"BOOT: MOUSE_Init returned %d; KEYBOARD_Init",result);
    startup_step(status);
    if(result<0) WiiOnline_Fatal("Cannot initialize SDL USB mouse event queue.");
    result=KEYBOARD_Init(NULL);
    snprintf(status,sizeof(status),"BOOT: KEYBOARD_Init returned %d",result);
    startup_step(status);
    if(result<0) WiiOnline_Fatal("Cannot initialize SDL USB keyboard event queue.");
    startup_step("BOOT: controllers ready; initializing TLS entropy");
    if (WiiOnline_InitRandom())
        WiiOnline_Fatal("TLS entropy initialization failed. Provision a private TLS seed for this Wii installation using provision_tls_seed.py.");
    startup_step("BOOT: TLS entropy ready; configuring network (DHCP)");
    init_network();
}
int __real_SDL_main(int argc, char **argv);
/* Own the native entry point: SDL2main otherwise initializes IOS, WPAD,
 * keyboard, mouse and FAT before this bootstrap, duplicating setup and hiding
 * any early hang behind a black screen. libogc CRT still owns CPU/IOS startup. */
int main(int argc, char **argv)
{
    char *online_args[16];
    char module_path[1024];
    /* Retain argv[0] on our MEM1 stack across IOS reload/device reservations. */
    snprintf(module_path,sizeof(module_path),"%s",argc && argv && argv[0] ? argv[0] : "client.dol");
    online_args[0]=module_path;
    /* FTE's stock worker shutdown spins while waiting for loader threads.
     * Use its supported synchronous loader mode on the single-core Wii. */
    online_args[1]="-noworkers";
    /* Loose FAT files already have case-insensitive lookups. Avoid recursive
     * whole-tree hash rebuilds while an accepted server waits for signon. */
    online_args[2]="+set";
    online_args[3]="fs_cache";
    online_args[4]="0";
    online_args[5]="+set";
    online_args[6]="cl_download_wait";
    online_args[7]="0";
    online_args[8]="+set";
    online_args[9]="mod_precache";
    online_args[10]="0";
    online_args[11]="+set";
    online_args[12]="s_precache";
    online_args[13]="0";
    online_args[14]="+menu_servers";
    online_args[15]=NULL;
    init_platform(1, online_args);
    WiiOnline_MemoryReport("before FTE engine startup");
    startup_step("BOOT: synchronous resource loading (-noworkers)");
    startup_step("BOOT: entering FTE SDL_main / Host_Init");
    int result=__real_SDL_main(15, online_args);
    log_runtime=0;flush_online_log();
    return result;
}
