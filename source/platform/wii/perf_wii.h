#ifndef NZP_PERF_WII_H
#define NZP_PERF_WII_H
/* CPU wall times; nested scopes must not be added together. No GPU drain. */
enum { WII_PERF_SERVER, WII_PERF_PHYSICS, WII_PERF_CLIENT,
       WII_PERF_SCREEN, WII_PERF_PRESENT, WII_PERF_AUDIO, WII_PERF_ENTITIES,
       WII_PERF_COUNT };
void Wii_PerfInit(void);
void Wii_PerfBeginFrame(void);
void Wii_PerfEndFrame(void);
double Wii_PerfClock(void);
void Wii_PerfAdd(int scope, double begin);
void Wii_PerfAlias(double begin, double built, int hit, int vertices);
#endif
