#include "ftCo_SpecialAir.h"
#include <melee/rogue/rogue_ability.h>

#include <Runtime/platform.h>

#include <dolphin/mtx.h>
#include <melee/ft/fighter.h>
#include <melee/ft/ftcommon.h>
#include <melee/ft/ftdata.h>
#include <melee/ft/types.h>

bool ftCo_SpecialAir_CheckInput(Fighter_GObj* gobj)
{
    Fighter* fp = gobj->user_data;
    if (fp->input.pressed_buttons & HSD_PAD_B) {
        if (fp->input.lstick[0].y >= p_ftCommonData->x21C) {
            if (ftData_SpecialAirHi[fp->kind] == NULL) {
                return false;
            }
            if (!Rogue_TrySpecial(gobj, ROGUE_ABILITY_UP, true))
                ftData_SpecialAirHi[fp->kind](gobj);
            fp->x2227_b5 = true;
            return true;
        }
        if (fp->input.lstick[0].y <= -p_ftCommonData->x21C) {
            if (ftData_SpecialAirLw[fp->kind] == NULL) {
                return false;
            }
            if (!Rogue_TrySpecial(gobj, ROGUE_ABILITY_DOWN, true))
                ftData_SpecialAirLw[fp->kind](gobj);
            fp->x2227_b5 = true;
            return true;
        }
        if (ABS(fp->input.lstick[0].x) >= p_ftCommonData->x218) {
            if (ftData_SpecialAirS[fp->kind] == NULL) {
                return false;
            }
            if (fp->input.lstick[0].x * fp->facing_dir < -p_ftCommonData->x220)
            {
                ftCommon_UpdateFacing(fp);
            }
            if (!Rogue_TrySpecial(gobj, ROGUE_ABILITY_SIDE, true))
                ftData_SpecialAirS[fp->kind](gobj);
            fp->x2227_b5 = true;
            return true;
        }
        if (ftData_SpecialAirN[fp->kind] == NULL) {
            return false;
        }
        if (fp->active_duration.lstick.x < p_ftCommonData->x224 &&
            ((fp->facing_dir == -1 && fp->x2228_b7 == 1) ||
             (fp->facing_dir == +1 && fp->x2228_b7 == 0)))
        {
            fp->facing_dir = -fp->facing_dir;
        }
        if (!Rogue_TrySpecial(gobj, ROGUE_ABILITY_NEUTRAL, true))
            ftData_SpecialAirN[fp->kind](gobj);
        fp->x2227_b5 = true;
        return true;
    }
    return false;
}
