/* GPL-2.0-or-later. No ABI change; only the Wii client links this callback. */
#ifndef QCLIB_WII_COOPERATE_H
#define QCLIB_WII_COOPERATE_H
#if defined(PLATFORM_WII) && defined(CLIENTONLY)
void CL_WiiQCLibPoll(pubprogfuncs_t *vm);
#define WII_QC_COOPERATE(vm,step,mask) do { \
    if (!((unsigned int)(step)&(mask))) CL_WiiQCLibPoll(vm); \
} while (0)
#else
#define WII_QC_COOPERATE(vm,step,mask) ((void)0)
#endif
#endif
