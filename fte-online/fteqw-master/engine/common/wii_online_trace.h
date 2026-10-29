#ifndef WII_ONLINE_TRACE_H
#define WII_ONLINE_TRACE_H
#ifdef PLATFORM_WII
void WiiOnline_Log(const char *text);
#define WII_STARTUP_TRACE(text) WiiOnline_Log("GFX: " text "\n")
#else
#define WII_STARTUP_TRACE(text) ((void)0)
#endif
#endif
