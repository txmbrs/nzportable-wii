/* GPL-2.0-or-later. Keep only the background and selected map preview resident. */
#include "../../nzportable_def.h"
#include "../../menu/menu_defs.h"
#include "menu_images_wii.h"
#include <gccore.h>

static struct {int map, texture;} slots[2]={{-1,-1},{-1,-1}};
static char *identifiers[2]={"wii_menu_preview","wii_menu_background"};

static void ReleaseSlot(int slot)
{
    int texture=Hyena_FindTexture(identifiers[slot]);
    /* The registry may have been cleared at map load. Never delete a reused
     * numeric ID, and never free texels while GX can still sample them. */
    if (texture>=0) {
        GX_DrawDone();
        Hyena_DestroyTexture(texture);
        GX_InvalidateTexAll();
    }
    slots[slot].map=slots[slot].texture=-1;
}
void Wii_MenuImagesReset(void)
{
    ReleaseSlot(0); ReleaseSlot(1);
    menu_background=menu_bk;
    menu_changetime=0;
}
int Wii_MenuMapImage(int index, int background)
{
    int slot=background?1:0;
    if (index<0 || index>=num_user_maps || !custom_maps[index].map_use_thumbnail)
        return -1;
    if (slots[slot].map==index) {
        if (slots[slot].texture<0) return -1; /* Cache a missing file too. */
        if (Hyena_FindTexture(identifiers[slot])==slots[slot].texture)
            return slots[slot].texture;
    }
    ReleaseSlot(slot);
    slots[slot].map=index;
    slots[slot].texture=Image_LoadImageWithIdentifier(custom_maps[index].map_thumbnail_path,
        identifiers[slot],IMAGE_PNG|IMAGE_TGA|IMAGE_JPG,1,false,false);
    return slots[slot].texture;
}
