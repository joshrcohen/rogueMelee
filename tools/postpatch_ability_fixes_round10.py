#!/usr/bin/env python3
from pathlib import Path
import sys

root = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else Path.cwd()
path = root / "src/melee/ft/kinds/ftPeach/ftpeachspecialhi.c"
if not path.is_file():
    raise SystemExit(f"Missing expected file: {path}")

text = path.read_text(encoding="utf-8")

old = '''    if (!checkCmdVar2(gobj) && !ftAnim_IsFramesRemaining(gobj)) {
        Fighter* fp = GET_FIGHTER(gobj);
        ftPe_DatAttrs* da = fp->dat_attrs;
        if (fp->mv.pe.specialhi.kind == It_Kind_Parasol) {
            fp->motion_id = ftCo_MS_FallSpecial;
            ftCo_800CEFE0(gobj, p_ftCommonData->x59C);
        } else {
            fp->motion_id = ftCo_MS_FallSpecial;
            ftCo_800CEFE0(gobj, da->x90);
        }
    }
'''

new = '''    if (!checkCmdVar2(gobj) && !ftAnim_IsFramesRemaining(gobj)) {
        Fighter* fp = GET_FIGHTER(gobj);
        ftPe_DatAttrs* da = fp->dat_attrs;

        /*
         * Native Peach transitions from Up-B into ItemParasolOpen. That common
         * state owns a parasol-specific fighter script (including event 0x2A)
         * and assumes a real Peach/Parasol owner relationship.
         *
         * On a borrowed non-Peach move, Rogue_AbilityMotionState() correctly
         * tears down the source swap when we leave Peach's special-state range.
         * Entering ItemParasolOpen after that teardown leaves the recipient
         * running parasol-only script/data and hangs. Bypass the parasol state
         * entirely and enter the ordinary recipient-safe special-fall state.
         */
        if (Rogue_IsAbilityState(fp) &&
            Rogue_AbilitySourceKind(fp) == Ft_Kind_Peach &&
            fp->kind != Ft_Kind_Peach)
        {
            ftCo_80096900(gobj, 0, 1, false, da->x70, da->x74);
            return;
        }

        if (fp->mv.pe.specialhi.kind == It_Kind_Parasol) {
            fp->motion_id = ftCo_MS_FallSpecial;
            ftCo_800CEFE0(gobj, p_ftCommonData->x59C);
        } else {
            fp->motion_id = ftCo_MS_FallSpecial;
            ftCo_800CEFE0(gobj, da->x90);
        }
    }
'''

if new in text:
    print("round10: borrowed Peach Up-B transition already fixed")
elif old in text:
    text = text.replace(old, new, 1)
    path.write_text(text, encoding="utf-8", newline="\n")
    print("round10: bypass ItemParasolOpen for borrowed non-Peach Up-B")
else:
    raise SystemExit(
        "round10: expected ftPe_SpecialHiStart_Anim block not found"
    )

print("round10: postpatch applied")
