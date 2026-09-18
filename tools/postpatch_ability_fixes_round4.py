#!/usr/bin/env python3
from pathlib import Path
import sys

root = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else Path.cwd()
path = root / "src/melee/ft/ftparts.c"
if not path.is_file():
    raise SystemExit(f"Missing expected file: {path}")

text = path.read_text(encoding="utf-8")

inc_anchor = '#include "ftparts.h"\n'
inc = '#include <melee/rogue/rogue_ability.h>\n#include <melee/rogue/rogue_debug.h>\n'
if inc not in text:
    if inc_anchor not in text:
        raise SystemExit("ftparts.c include anchor not found")
    text = text.replace(inc_anchor, inc_anchor + inc, 1)

old = '''Fighter_Part ftParts_GetBoneIndex(Fighter* fp, Fighter_Part part)
{
    return ftPartsTable[fp->kind]->part_to_joint[part];
}
'''

new = '''Fighter_Part ftParts_GetBoneIndex(Fighter* fp, Fighter_Part part)
{
    FighterPartsTable* table = ftPartsTable[fp->kind];
    Fighter_Part joint = table->part_to_joint[part];

    if (Rogue_IsAbilityState(fp) &&
        (joint == FTPART_INVALID || joint >= table->parts_num ||
         fp->parts[joint].joint == NULL))
    {
        Fighter_Part fallback = FtPart_TransN;

        if (part >= FtPart_L1stNa && part <= FtPart_LHandNb)
            fallback = FtPart_LHandN;
        else if (part >= FtPart_R1stNa && part <= FtPart_RHandNb)
            fallback = FtPart_RHandN;

        joint = table->part_to_joint[fallback];
        if (joint == FTPART_INVALID || joint >= table->parts_num ||
            fp->parts[joint].joint == NULL)
        {
            joint = table->part_to_joint[FtPart_TransN];
        }

        Rogue_DebugCheckpointEvent("bone fallback", (u32) part);
    }

    return joint;
}
'''

if new not in text:
    if old not in text:
        raise SystemExit("ftParts_GetBoneIndex() source did not match expected pinned Melee source")
    text = text.replace(old, new, 1)

path.write_text(text, encoding="utf-8", newline="\n")
print("round4: installed safe borrowed-special common-bone fallback")
