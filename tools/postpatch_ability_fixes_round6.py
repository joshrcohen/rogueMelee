#!/usr/bin/env python3
from pathlib import Path
import sys

root = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else Path.cwd()
path = root / "src/melee/ft/ftafterimage.c"
if not path.is_file():
    raise SystemExit(f"Missing expected file: {path}")

text = path.read_text(encoding="utf-8")

inc_anchor = '#include "ftafterimage.h"\n'
inc = '#include <melee/rogue/rogue_ability.h>\n#include <melee/rogue/rogue_debug.h>\n'
if inc not in text:
    if inc_anchor not in text:
        raise SystemExit("ftafterimage.c include anchor not found")
    text = text.replace(inc_anchor, inc_anchor + inc, 1)

old = '''static inline itSword_UnkBytes* ftCo_800C2600_get_params(Fighter* fp)
{
    itSword_UnkBytes* params;

    if (fp->x2101_bits_8) {
'''
new = '''static inline itSword_UnkBytes* ftCo_800C2600_get_params(Fighter* fp)
{
    itSword_UnkBytes* params = NULL;
    FighterKind kind = fp->kind;

    if (Rogue_IsAbilityState(fp))
        kind = Rogue_AbilitySourceKind(fp);

    if (fp->x2101_bits_8) {
'''
if new not in text:
    if old not in text:
        raise SystemExit("get_params declaration block not found")
    text = text.replace(old, new, 1)

# Replace the first non-item fighter-kind switch in get_params only.
old = '''    } else {
        switch (fp->kind) {
        case Ft_Kind_Seak:
'''
new = '''    } else {
        switch (kind) {
        case Ft_Kind_Seak:
'''
if new not in text:
    if old not in text:
        raise SystemExit("get_params fighter-kind switch not found")
    text = text.replace(old, new, 1)

old = '''    params = ftCo_800C2600_get_params(fp);

    {
'''
new = '''    params = ftCo_800C2600_get_params(fp);
    if (params == NULL) {
        fp->x2100 = -1;
        Rogue_DebugCheckpoint("afterimage disabled");
        return;
    }

    {
'''
if new not in text:
    if old not in text:
        raise SystemExit("afterimage render params call not found")
    text = text.replace(old, new, 1)

old = '''void ftCo_800C2FD8(Fighter_GObj* gobj)
{
    Fighter* fp;
    HSD_JObj* jobj;
    struct Fighter_x20B0_t* entry;
    int axis;
    struct SwordAttrs* attrs;
    int nextIndex;
    PAD_STACK(0x8);

    fp = GET_FIGHTER(gobj);
    if (fp->x2100 == -1) {
'''
new = '''void ftCo_800C2FD8(Fighter_GObj* gobj)
{
    Fighter* fp;
    HSD_JObj* jobj;
    struct Fighter_x20B0_t* entry;
    int axis;
    struct SwordAttrs* attrs;
    int nextIndex;
    FighterKind kind;
    PAD_STACK(0x8);

    fp = GET_FIGHTER(gobj);
    kind = fp->kind;
    if (Rogue_IsAbilityState(fp))
        kind = Rogue_AbilitySourceKind(fp);

    if (fp->x2100 == -1) {
'''
if new not in text:
    if old not in text:
        raise SystemExit("ftCo_800C2FD8 declaration block not found")
    text = text.replace(old, new, 1)

# This should replace the second fighter-kind switch (the first was changed above).
old = '''    } else {
        switch (fp->kind) {
        case Ft_Kind_Seak:
'''
new = '''    } else {
        switch (kind) {
        case Ft_Kind_Seak:
'''
if new not in text:
    if old not in text:
        raise SystemExit("ftCo_800C2FD8 fighter-kind switch not found")
    text = text.replace(old, new, 1)

# Make the unsupported/non-sword path safe instead of preserving upstream UB.
old = '''        case Ft_Kind_GameWatch:
        case Ft_Kind_Ganon:
            /// @bug Undefined behavior if the fighter doesn't have a sword!
            break;
'''
new = '''        case Ft_Kind_GameWatch:
        case Ft_Kind_Ganon:
            /*
             * Upstream intentionally has undefined behavior here because native
             * gameplay only enables this path for fighters with sword data.
             * Borrowed subactions can enable it on any recipient, so disable
             * the trail instead of using an uninitialized attrs pointer.
             */
            fp->x2100 = -1;
            Rogue_DebugCheckpoint("afterimage unsupported");
            return;
'''
# There are two similar switches; only the update function contains the @bug comment.
if new not in text:
    if old not in text:
        raise SystemExit("afterimage upstream UB block not found")
    text = text.replace(old, new, 1)

# The original switch already has a default label. Make that default safe too.
old = '''        default:
            break;
        }
        axis = 0;
'''
new = '''        default:
            fp->x2100 = -1;
            Rogue_DebugCheckpoint("afterimage unsupported");
            return;
        }
        axis = 0;
'''
if new not in text:
    if old not in text:
        raise SystemExit("afterimage existing default block not found")
    text = text.replace(old, new, 1)

old = '''        axis = 0;
        fp->x20F8 = attrs->x18;
        fp->x20FC = attrs->x1C;
        jobj = fp->parts[attrs->x14].joint;
    }
    lb_8000B1CC(jobj, NULL, &entry->x0);
'''
new = '''        axis = 0;
        fp->x20F8 = attrs->x18;
        fp->x20FC = attrs->x1C;
        if (Rogue_IsAbilityState(fp))
            jobj = fp->parts[Rogue_AbilityMapBone(fp, attrs->x14)].joint;
        else
            jobj = fp->parts[attrs->x14].joint;
    }

    if (jobj == NULL) {
        fp->x2100 = -1;
        Rogue_DebugCheckpoint("afterimage missing joint");
        return;
    }

    Rogue_DebugCheckpoint("afterimage remapped");
    lb_8000B1CC(jobj, NULL, &entry->x0);
'''
if new not in text:
    if old not in text:
        raise SystemExit("afterimage sword-joint block not found")
    text = text.replace(old, new, 1)

path.write_text(text, encoding="utf-8", newline="\n")
print("round6: borrowed sword afterimage source-kind + bone remap fix applied")
