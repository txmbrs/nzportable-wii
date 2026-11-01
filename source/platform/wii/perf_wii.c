/* Wii CPU profiling for the NZPortable port. GPL-2.0-or-later. */
#include "../../nzportable_def.h"
#include "perf_wii.h"

/* One summary per 600 active frames. Set wii_perf 1 to enable profiling. */
static cvar_t wii_perf = {"wii_perf", "0"};
static qboolean collecting;
static unsigned frames, calls, hits, vertices_total;
static double frame_begin, frame_sum, frame_max, sums[WII_PERF_COUNT];
static double alias_build, alias_draw;
void Wii_PerfInit(void) { Cvar_RegisterVariable(&wii_perf); }
double Wii_PerfClock(void) { return collecting ? Sys_FloatTime() : 0; }
void Wii_PerfBeginFrame(void)
{
    collecting = wii_perf.value != 0 && cls.state == ca_connected && cl.worldmodel != NULL;
    if (collecting) frame_begin=Sys_FloatTime();
    else {
        frames=calls=hits=vertices_total=0;
        frame_sum=frame_max=alias_build=alias_draw=0;
        memset(sums,0,sizeof(sums));
    }
}
void Wii_PerfAdd(int scope, double begin)
{
    if (collecting && begin && scope>=0 && scope<WII_PERF_COUNT)
        sums[scope]+=Sys_FloatTime()-begin;
}
void Wii_PerfAlias(double begin, double built, int hit, int vertices)
{
    if (!collecting || !begin) return;
    alias_build+=built-begin;
    alias_draw+=Sys_FloatTime()-built;
    ++calls; hits+=hit!=0; vertices_total+=vertices;
}
void Wii_PerfEndFrame(void)
{
    double elapsed, ms;
    if (!collecting) return;
    elapsed=Sys_FloatTime()-frame_begin;
    frame_sum+=elapsed;
    if (elapsed>frame_max) frame_max=elapsed;
    if (++frames<600) return;
    ms=1000.0/frames;
    Sys_Printf("WII PERF frames=%u work_ms=%.2f max=%.2f server=%.2f physics=%.2f client=%.2f screen=%.2f present=%.2f audio=%.2f entities=%.2f alias_build=%.2f alias_draw=%.2f models/frame=%.1f verts/frame=%.0f pose_hits=%u/%u\n",
        frames,frame_sum*ms,frame_max*1000,
        sums[WII_PERF_SERVER]*ms,sums[WII_PERF_PHYSICS]*ms,
        sums[WII_PERF_CLIENT]*ms,sums[WII_PERF_SCREEN]*ms,
        sums[WII_PERF_PRESENT]*ms,sums[WII_PERF_AUDIO]*ms,sums[WII_PERF_ENTITIES]*ms,
        alias_build*ms,alias_draw*ms,(double)calls/frames,(double)vertices_total/frames,hits,calls);
    frames=calls=hits=vertices_total=0;
    frame_sum=frame_max=alias_build=alias_draw=0;
    memset(sums,0,sizeof(sums));
}
