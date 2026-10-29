#ifndef NZP_CHAINLOADER_WII_H
#define NZP_CHAINLOADER_WII_H
int Wii_CheckDOL(const char *path,char *error,unsigned int capacity);
void Wii_ChainloadSetLogger(void (*logger)(const char *));
int Wii_ChainloadDOL(const char *path,char *error,unsigned int capacity);
/* Only after engine and SDL shutdown: reuse the caller's retired MEM2 hunk. */
int Wii_ChainloadDOLWithBuffer(const char *path,void *buffer,unsigned int bytes,char *error,unsigned int capacity);
#endif
