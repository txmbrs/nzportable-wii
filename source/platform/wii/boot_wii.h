#ifndef NZP_BOOT_WII_H
#define NZP_BOOT_WII_H
#define NZP_WII_REVISION "single-player-vbo-r22"
void Wii_BootInit(int argc,char **argv);
void Wii_BootMessage(const char *message);
void Wii_BootGraphicsReady(void);
void Wii_BootFatal(const char *message);
#endif








