#!/usr/bin/env python3
from pathlib import Path
import sys

root = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else Path.cwd()
path = root / "src/melee/ft/ftaction.c"
if not path.is_file():
    raise SystemExit(f"Missing expected file: {path}")

text = path.read_text(encoding="utf-8")

old = '''void ftAction_80072894(Fighter_GObj* gobj, CommandInfo* cmd)
{
    ftCommon_8007E83C(gobj, cmd->u->unk9.unk1, cmd->u->unk9.unk2);
    NEXT_CMD(cmd);
}
'''

new = '''void ftAction_80072894(Fighter_GObj* gobj, CommandInfo* cmd)
{
    Fighter* fp = GET_FIGHTER(gobj);

    /*
     * Subaction event 0x2A drives native parasol article animation. The
     * borrowed Peach move already carries its gameplay state, movement,
     * hitboxes, and attached article separately. Running this native visual
     * controller on a non-Peach recipient still assumes Peach fighter/item
     * integration and hangs after the script reaches 0x2A.
     *
     * Keep native Peach untouched; for cross-character borrowed Peach moves,
     * skip only this visual article-animation event.
     */
    if (Rogue_IsAbilityState(fp) &&
        Rogue_AbilitySourceKind(fp) == Ft_Kind_Peach &&
        fp->kind != Ft_Kind_Peach)
    {
        NEXT_CMD(cmd);
        return;
    }

    ftCommon_8007E83C(gobj, cmd->u->unk9.unk1, cmd->u->unk9.unk2);
    NEXT_CMD(cmd);
}
'''

if new in text:
    print("round8: borrowed Peach parasol event already guarded")
elif old in text:
    text = text.replace(old, new, 1)
    path.write_text(text, encoding="utf-8", newline="\n")
    print("round8: skip native Parasol article-animation event on non-Peach recipients")
else:
    raise SystemExit(
        "round8: expected ftAction_80072894 body not found; "
        "inspect generated ftaction.c before continuing"
    )

print("round8: postpatch applied")
