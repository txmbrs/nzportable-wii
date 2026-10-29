#ifndef WII_ONLINE_NETTRACE_H
#define WII_ONLINE_NETTRACE_H
void WiiNetTrace_Record(unsigned event,unsigned stage,int result,int previous,
                        int submission,unsigned retries,int payload,unsigned operation);
void WiiNetTrace_Reset(void);
void WiiNetTrace_Drain(void);
void WiiNetTrace_Status(void);
#endif
