#include <gccore.h>
#include <fat.h>
#include <wiiuse/wpad.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "boot_wii.h"

static GXRModeObj *boot_mode;
static void *boot_framebuffer;
static FILE *boot_log;
static int console_active;
static void show_console(void){
    VIDEO_Configure(boot_mode);
    VIDEO_SetNextFramebuffer(boot_framebuffer);
    console_init(boot_framebuffer,20,20,boot_mode->fbWidth-40,boot_mode->xfbHeight-40,boot_mode->fbWidth*2);
    VIDEO_SetBlack(FALSE);VIDEO_Flush();VIDEO_WaitVSync();
    if(boot_mode->viTVMode&VI_NON_INTERLACE)VIDEO_WaitVSync();
    console_active=1;
}
void Wii_BootMessage(const char *message){
    if(boot_log){fputs(message,boot_log);fflush(boot_log);}
    if(console_active){fputs(message,stdout);fflush(stdout);}
}
void Wii_BootGraphicsReady(void){Wii_BootMessage("BOOT: graphics context ready\n");console_active=0;}
void Wii_BootFatal(const char *message){
    if(boot_framebuffer)show_console();
    Wii_BootMessage("\nNZPortable startup error:\n");Wii_BootMessage(message);
    Wii_BootMessage("\nBuild: " NZP_WII_REVISION "\n");
    Wii_BootMessage("\n\nSee wii-startup.txt beside boot.dol.\nHOME / START: exit\n");
    if(boot_framebuffer)for(;;){
        WPAD_ScanPads();PAD_ScanPads();
        if((WPAD_ButtonsDown(0)&WPAD_BUTTON_HOME)||(PAD_ButtonsDown(0)&PAD_BUTTON_START))break;
        VIDEO_WaitVSync();
    }
    if(boot_log){fclose(boot_log);boot_log=NULL;}
    exit(1);
}
static int data_here(void){struct stat st;return !stat("nzp",&st)&&S_ISDIR(st.st_mode);}
void Wii_BootInit(int argc,char **argv){
    VIDEO_Init();boot_mode=VIDEO_GetPreferredMode(NULL);
    void *allocated_framebuffer=SYS_AllocateFramebuffer(boot_mode);
    if(!allocated_framebuffer)exit(1);
    boot_framebuffer=MEM_K0_TO_K1(allocated_framebuffer);
    show_console();Wii_BootMessage("NZPortable Wii - " NZP_WII_REVISION "\nMounting SD / USB...\n");
    WPAD_Init();PAD_Init();WPAD_SetIdleTimeout(3600);
    if(!fatInitDefault())Wii_BootFatal("Cannot mount SD / USB. Enable/insert an SD card with apps/nzportable/nzp.");
    char original[1024],directory[1024];
    if(!getcwd(original,sizeof(original)))strcpy(original,"/");
    int found=0;
    if(argc>0&&argv&&argv[0]&&strstr(argv[0],":/")&&strlen(argv[0])<sizeof(directory)){
        strcpy(directory,argv[0]);char *slash=strrchr(directory,'/');
        if(slash){*slash=0;found=!chdir(directory)&&data_here();}
    }
    if(!found)found=!chdir(original)&&data_here();
    if(!found)found=!chdir("sd:/apps/nzportable")&&data_here();
    if(!found)found=!chdir("usb:/apps/nzportable")&&data_here();
    if(!found)Wii_BootFatal("Game data not found. Keep the entire nzp folder next to boot.dol.");
    boot_log=fopen("wii-startup.txt","w");
    Wii_BootMessage("BOOT: SD / USB mounted; game directory selected\n");
    if(getcwd(directory,sizeof(directory))){Wii_BootMessage(directory);Wii_BootMessage("\n");}
    if(!boot_log)Wii_BootMessage("Warning: cannot write startup log on this device.\n");
}
