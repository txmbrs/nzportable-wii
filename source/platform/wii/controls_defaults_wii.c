/* GPL-2.0-or-later. Explicit reset of controls without touching video/audio. */
#include "../../nzportable_def.h"
#include "../../menu/menu_defs.h"
#include "controls_defaults_wii.h"
extern int bindings_currentwatch;
void Wii_ResetControls_f(void);

void Wii_ResetControls_f(void)
{
    unsigned i;
    if (cmd_source!=src_command) return;
    in_bind=false;
    bindings_currentwatch=-1;
    /* Release actions using their OLD bindings before replacing them. */
    for (i=0;i<MAX_KEYS;++i) if (keydown[i]) Key_Event(i,false);
    Key_ClearStates();
    IN_PlatformClearPendingInput();
    for (i=0;i<MAX_KEYS;++i) {
        Key_SetBinding(i,"");
        Key_SetDTBinding(i,"");
        Key_SetHoldBinding(i,"");
    }
    for (i=0;i<sizeof(wii_factory_bindings)/sizeof(wii_factory_bindings[0]);++i) {
        int key=Key_StringToKeynum((char *)wii_factory_bindings[i].key);
        if (wii_factory_bindings[i].hold)
            Key_SetHoldBinding(key,(char *)wii_factory_bindings[i].command);
        else Key_SetBinding(key,(char *)wii_factory_bindings[i].command);
    }
    for (i=0;i<sizeof(wii_factory_controls)/sizeof(wii_factory_controls[0]);++i)
        Cvar_Set(wii_factory_controls[i].name,(char *)wii_factory_controls[i].value);
    if (Host_WriteConfiguration()) Sys_Printf("Wii controls: factory defaults restored and saved\n");
    else Sys_Printf("Wii controls: defaults restored; configuration save failed\n");
}
