#!/usr/bin/env python3
from pathlib import Path
import sys

root = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else Path.cwd()


def load(rel):
    path = root / rel
    if not path.is_file():
        raise SystemExit(f"round12: missing expected file: {path}")
    return path, path.read_text(encoding="utf-8")


def save(path, text):
    path.write_text(text, encoding="utf-8", newline="\n")


def ensure_include(text, inc, label):
    if inc in text:
        return text
    lines = text.splitlines(keepends=True)
    for i, line in enumerate(lines):
        if line.startswith("#include "):
            lines.insert(i + 1, inc)
            return "".join(lines)
    raise SystemExit(f"round12: could not add include to {label}")


# FIX 1: Physical Kirby infrastructure must always read Kirby's native union.
p, s = load("src/melee/ft/kinds/ftKirby/ftkirby.c")
s = ensure_include(
    s,
    "#include <melee/rogue/rogue_ability.h>\n",
    "ftkirby.c",
)

replacements = [
(
'''void ftKb_UnkMtxFunc0(Fighter_GObj* gobj, int arg1, Mtx mtx)
{
    Fighter* fp = gobj->user_data;

    if (fp->u.kb.hat.jobj == NULL) {
        return;
    }
    if (!fp->x2225_b2) {
        return;
    }

    {
        MtxPtr bone_mtx = HSD_JObjGetMtxPtr(fp->parts[6].joint);
        HSD_JObj* jobj = fp->u.kb.hat.jobj;
        HSD_JObjCopyMtx(fp->u.kb.hat.jobj, bone_mtx);
        jobj->flags |=
            JOBJ_USER_DEF_MTX | JOBJ_MTX_INDEP_PARENT | JOBJ_MTX_INDEP_SRT;
        HSD_JObjSetMtxDirty(jobj);
        HSD_JObjDispAll(fp->u.kb.hat.jobj, mtx, HSD_GObj_80390EB8(arg1), 0);
    }
    PAD_STACK(8);
}
''',
'''void ftKb_UnkMtxFunc0(Fighter_GObj* gobj, int arg1, Mtx mtx)
{
    Fighter* fp = gobj->user_data;
    union Fighter_FighterVars* vars =
        Rogue_AbilityVars(fp, Ft_Kind_Kirby);

    if (vars->kb.hat.jobj == NULL) {
        return;
    }
    if (!fp->x2225_b2) {
        return;
    }

    {
        MtxPtr bone_mtx = HSD_JObjGetMtxPtr(fp->parts[6].joint);
        HSD_JObj* jobj = vars->kb.hat.jobj;
        HSD_JObjCopyMtx(vars->kb.hat.jobj, bone_mtx);
        jobj->flags |=
            JOBJ_USER_DEF_MTX | JOBJ_MTX_INDEP_PARENT | JOBJ_MTX_INDEP_SRT;
        HSD_JObjSetMtxDirty(jobj);
        HSD_JObjDispAll(vars->kb.hat.jobj, mtx, HSD_GObj_80390EB8(arg1), 0);
    }
    PAD_STACK(8);
}
''',
"Kirby matrix/render callback"
),
(
'''HSD_JObj* ftKb_Init_UnkMotionStates6(Fighter_GObj* gobj)
{
    Fighter* fp = GET_FIGHTER(gobj);

    if (fp->u.kb.hat.jobj) {
        return fp->u.kb.hat.jobj;
    }

    /// @remark This is actually a correctness hack. If no return was here,
    /// MWCC emits the same code.
    return (HSD_JObj*) gobj;
}
''',
'''HSD_JObj* ftKb_Init_UnkMotionStates6(Fighter_GObj* gobj)
{
    Fighter* fp = GET_FIGHTER(gobj);
    union Fighter_FighterVars* vars =
        Rogue_AbilityVars(fp, Ft_Kind_Kirby);

    if (vars->kb.hat.jobj) {
        return vars->kb.hat.jobj;
    }

    /// @remark This is actually a correctness hack. If no return was here,
    /// MWCC emits the same code.
    return (HSD_JObj*) gobj;
}
''',
"Kirby model getter"
),
(
'''void ftKb_UnkIntBoolFunc0(Fighter* fp, int arg1, bool arg2)
{
    if (fp->u.kb.hat.x14.data != NULL) {
        if (arg2) {
            if (fp->u.kb.hat.jobj != NULL) {
                ftParts_80074CA0(&fp->u.kb.hat.x24, arg1, &fp->u.kb.hat.x14);
                return;
            }
            {
                s32 var_r0;
                s8 idx = fp->x5F4_arr[0].idx;
                if (idx >= 2 && idx <= 6) {
                    var_r0 = 1;
                } else {
                    var_r0 = 0;
                }
                if (var_r0 != 0) {
                    ftParts_80074D7C(&fp->u.kb.hat.x24, arg1,
                                     &fp->u.kb.hat.x14);
                    return;
                }
            }
            if (arg1 == 2) {
                if (*fp->u.kb.hat.x1C.data != NULL) {
                    ftParts_80074D7C(&fp->x5AC, arg1, &fp->x203C);
                    ftParts_80074CA0(&fp->u.kb.hat.x24, arg1,
                                     &fp->u.kb.hat.x1C);
                }
            } else {
                ftParts_80074D7C(&fp->x5AC, arg1, &fp->dobj_list);
                ftParts_80074CA0(&fp->u.kb.hat.x24, arg1, &fp->u.kb.hat.x14);
            }
        } else {
            if (fp->u.kb.hat.jobj == NULL && arg1 == 2) {
                ftParts_80074D7C(&fp->u.kb.hat.x24, arg1, &fp->u.kb.hat.x1C);
                return;
            }
            ftParts_80074D7C(&fp->u.kb.hat.x24, arg1, &fp->u.kb.hat.x14);
        }
    }
}
''',
'''void ftKb_UnkIntBoolFunc0(Fighter* fp, int arg1, bool arg2)
{
    union Fighter_FighterVars* vars =
        Rogue_AbilityVars(fp, Ft_Kind_Kirby);

    if (vars->kb.hat.x14.data != NULL) {
        if (arg2) {
            if (vars->kb.hat.jobj != NULL) {
                ftParts_80074CA0(&vars->kb.hat.x24, arg1, &vars->kb.hat.x14);
                return;
            }
            {
                s32 var_r0;
                s8 idx = fp->x5F4_arr[0].idx;
                if (idx >= 2 && idx <= 6) {
                    var_r0 = 1;
                } else {
                    var_r0 = 0;
                }
                if (var_r0 != 0) {
                    ftParts_80074D7C(&vars->kb.hat.x24, arg1,
                                     &vars->kb.hat.x14);
                    return;
                }
            }
            if (arg1 == 2) {
                if (*vars->kb.hat.x1C.data != NULL) {
                    ftParts_80074D7C(&fp->x5AC, arg1, &fp->x203C);
                    ftParts_80074CA0(&vars->kb.hat.x24, arg1,
                                     &vars->kb.hat.x1C);
                }
            } else {
                ftParts_80074D7C(&fp->x5AC, arg1, &fp->dobj_list);
                ftParts_80074CA0(&vars->kb.hat.x24, arg1, &vars->kb.hat.x14);
            }
        } else {
            if (vars->kb.hat.jobj == NULL && arg1 == 2) {
                ftParts_80074D7C(&vars->kb.hat.x24, arg1, &vars->kb.hat.x1C);
                return;
            }
            ftParts_80074D7C(&vars->kb.hat.x24, arg1, &vars->kb.hat.x14);
        }
    }
}
''',
"Kirby model visibility callback"
),
(
'''void ftKb_Init_UnkCallbackPairs0_0(Fighter_GObj* gobj)
{
    Fighter* fp = GET_FIGHTER(gobj);
    if (fp->u.kb.hat.x14.data != NULL && fp->u.kb.hat.jobj == NULL) {
        ftAnim_800705E0(&fp->u.kb.x44);
    }
}
''',
'''void ftKb_Init_UnkCallbackPairs0_0(Fighter_GObj* gobj)
{
    Fighter* fp = GET_FIGHTER(gobj);
    union Fighter_FighterVars* vars =
        Rogue_AbilityVars(fp, Ft_Kind_Kirby);

    if (vars->kb.hat.x14.data != NULL && vars->kb.hat.jobj == NULL) {
        ftAnim_800705E0(&vars->kb.x44);
    }
}
''',
"Kirby model animation callback 0"
),
(
'''void ftKb_Init_UnkCallbackPairs0_1(Fighter_GObj* gobj, int arg1, float arg2)
{
    Fighter* fp = GET_FIGHTER(gobj);
    if (fp->u.kb.hat.x14.data != NULL && fp->u.kb.hat.jobj == NULL) {
        ftAnim_80070458(fp, &fp->u.kb.x44, arg1, arg2);
    }
}
''',
'''void ftKb_Init_UnkCallbackPairs0_1(Fighter_GObj* gobj, int arg1, float arg2)
{
    Fighter* fp = GET_FIGHTER(gobj);
    union Fighter_FighterVars* vars =
        Rogue_AbilityVars(fp, Ft_Kind_Kirby);

    if (vars->kb.hat.x14.data != NULL && vars->kb.hat.jobj == NULL) {
        ftAnim_80070458(fp, &vars->kb.x44, arg1, arg2);
    }
}
''',
"Kirby model animation callback 1"
),
]

for old, new, label in replacements:
    sig = old.splitlines()[0]
    start = s.find(sig)
    if start < 0:
        raise SystemExit(f"round12: function not found for {label}")
    window = s[start:start + max(len(old), len(new)) + 900]
    if "Rogue_AbilityVars(fp, Ft_Kind_Kirby)" in window:
        print(f"round12: {label} already native-state safe")
        continue
    if old not in s:
        raise SystemExit(f"round12: exact source block changed for {label}")
    s = s.replace(old, new, 1)
    print(f"round12: fixed {label}")

save(p, s)


# FIX 2: Sheik Chain cross-character pose layer.
p, s = load("src/melee/ft/kinds/ftSeak/ftseakspecials.c")

guard = '''    ftSk_SpecialS_80110490(fp);

    if (Rogue_IsAbilityState(fp) &&
        Rogue_AbilitySourceKind(fp) == Ft_Kind_Seak &&
        fp->kind != Ft_Kind_Seak)
    {
        return;
    }
'''

if "Rogue_AbilitySourceKind(fp) == Ft_Kind_Seak" not in s:
    anchor = "    ftSk_SpecialS_80110490(fp);\n"
    if anchor not in s:
        raise SystemExit("round12: Sheik Chain pose anchor not found")
    s = s.replace(anchor, guard, 1)
    print("round12: bypassed Sheik-only Chain pose blending on non-Sheik recipients")
else:
    print("round12: Sheik Chain cross-character pose guard already installed")

save(p, s)

print("round12: systemic Kirby native-union + Sheik Chain compatibility fixes applied")
