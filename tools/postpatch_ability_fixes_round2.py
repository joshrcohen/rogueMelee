#!/usr/bin/env python3
"""
Round 2 post-engine-patch fixes for RogueMelee borrowed-special hangs.
"""
from pathlib import Path
import sys

root = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else Path.cwd()

def load(rel):
    path = root / rel
    if not path.is_file():
        raise SystemExit(f"Missing expected file: {path}")
    return path, path.read_text(encoding="utf-8")

def save(path, text):
    path.write_text(text, encoding="utf-8", newline="\n")

def ensure_include(rel, anchor, include, label):
    path, text = load(rel)
    if include in text:
        return
    if anchor not in text:
        raise SystemExit(f"{label}: include anchor not found in {rel}")
    text = text.replace(anchor, anchor + include, 1)
    save(path, text)
    print(f"round2: {label}")

def replace_required(rel, old, new, label, count=1):
    path, text = load(rel)
    if new in text and old not in text:
        return
    if old not in text:
        if new in text:
            return
        raise SystemExit(f"{label}: expected text not found in {rel}")
    text = text.replace(old, new, count)
    save(path, text)
    print(f"round2: {label}")

ensure_include(
    "src/melee/ft/fighter.c",
    "#include <melee/rogue/rogue_ability.h>\n",
    "#include <melee/rogue/rogue_debug.h>\n",
    "include QA checkpoints in fighter.c",
)

replace_required(
    "src/melee/ft/fighter.c",
    """            fp->x594_s32 = unk_struct_x18->x10_animCurrFlags;
            ftCo_8009E7B4(fp, unk_byte_ptr);
""",
    """            fp->x594_s32 = unk_struct_x18->x10_animCurrFlags;
            Rogue_DebugCheckpoint("motion flags");
            if (Rogue_IsAbilityState(fp) &&
                Rogue_AbilitySourceKind(fp) != fp->kind) {
                Rogue_DebugCheckpoint("motion dynamics skipped");
            } else {
                ftCo_8009E7B4(fp, unk_byte_ptr);
                Rogue_DebugCheckpoint("motion dynamics done");
            }
""",
    "isolate source animation dynamics",
)

replace_required(
    "src/melee/ft/fighter.c",
    """                if (arg3 != 0U) {
                    ftData_80085CD8(fp, GET_FIGHTER(arg3), fp->anim_id);
                    ftColl_8007B8CC(fp, arg3);
                } else {
                    ftData_80085CD8(fp, fp, fp->anim_id);
                }
""",
    """                Rogue_DebugCheckpoint("motion anim load");
                if (arg3 != 0U) {
                    ftData_80085CD8(fp, GET_FIGHTER(arg3), fp->anim_id);
                    ftColl_8007B8CC(fp, arg3);
                } else {
                    ftData_80085CD8(fp, fp, fp->anim_id);
                }
                Rogue_DebugCheckpoint("motion anim loaded");
""",
    "trace animation archive load",
)

path, text = load("src/melee/ft/fighter.c")
needle = "                        ftAnim_8006EBE8(gobj,"
if 'Rogue_DebugCheckpoint("motion anim apply");\n                        ftAnim_8006EBE8(gobj,' not in text:
    if text.count(needle) < 2:
        raise SystemExit("trace animation apply: expected two calls")
    text = text.replace(
        needle,
        '                        Rogue_DebugCheckpoint("motion anim apply");\n' + needle,
        2,
    )
    terminator = """                                        : anim_blend ? anim_blend
                                                     : (*unk_byte_ptr)[0]);
"""
    if text.count(terminator) < 2:
        raise SystemExit("trace animation apply: terminator not found twice")
    text = text.replace(
        terminator,
        terminator + '                        Rogue_DebugCheckpoint("motion anim applied");\n',
        2,
    )
    save(path, text)
    print("round2: trace animation application")

ensure_include(
    "src/melee/ft/ftaction.c",
    "#include <melee/rogue/rogue_ability.h>\n",
    "#include <melee/rogue/rogue_debug.h>\n",
    "include QA checkpoints in ftaction.c",
)

replace_required(
    "src/melee/ft/ftaction.c",
    "                bone = fp->ft_data->x8->x12;",
    "                bone = Rogue_AbilityData(fp)->x8->x12;",
    "use source data for script GFX default bone",
)

replace_required(
    "src/melee/ft/ftaction.c",
    "    FtSFX* sfx = fp->ft_data->x4C_sfx;",
    "    FtSFX* sfx = Rogue_AbilityData(fp)->x4C_sfx;",
    "use source SFX table for borrowed scripts",
)

replace_required(
    "src/melee/ft/ftaction.c",
    """void ftAction_80071A9C(Fighter_GObj* gobj, CommandInfo* cmd)
{
    ftColl_8007B128(gobj, cmd->u->set_hurt_state.bone_idx,
                    cmd->u->set_hurt_state.state);
    NEXT_CMD(cmd);
}
""",
    """void ftAction_80071A9C(Fighter_GObj* gobj, CommandInfo* cmd)
{
    Fighter* fp = GET_FIGHTER(gobj);
    int bone = cmd->u->set_hurt_state.bone_idx;
    if (Rogue_IsAbilityState(fp))
        bone = Rogue_AbilityMapBone(fp, bone);
    ftColl_8007B128(gobj, bone, cmd->u->set_hurt_state.state);
    NEXT_CMD(cmd);
}
""",
    "remap script hurt-state bone",
)

replace_required(
    "src/melee/ft/ftaction.c",
    """void ftAction_80071D40(Fighter_GObj* gobj, CommandInfo* cmd)
{
    ftParts_80074B0C(gobj, cmd->u->set_dobj_flags.idx,
                     cmd->u->set_dobj_flags.value);
    NEXT_CMD(cmd);
}
""",
    """void ftAction_80071D40(Fighter_GObj* gobj, CommandInfo* cmd)
{
    Fighter* fp = GET_FIGHTER(gobj);
    if (!Rogue_IsAbilityState(fp) ||
        Rogue_AbilitySourceKind(fp) == fp->kind) {
        ftParts_80074B0C(gobj, cmd->u->set_dobj_flags.idx,
                         cmd->u->set_dobj_flags.value);
    }
    NEXT_CMD(cmd);
}
""",
    "suppress source-only DObj visibility commands",
)

replace_required(
    "src/melee/ft/ftaction.c",
    "                part = fp->ft_data->x8->x13;",
    "                part = Rogue_AbilityData(fp)->x8->x13;",
    "use source footstep bone A",
)
replace_required(
    "src/melee/ft/ftaction.c",
    "                part = fp->ft_data->x8->x14;",
    "                part = Rogue_AbilityData(fp)->x8->x14;",
    "use source footstep bone B",
)

replace_required(
    "src/melee/ft/ftaction.c",
    """    idx = cmd->u->wind_fx_0.bone;
    NEXT_CMD(cmd);
""",
    """    idx = cmd->u->wind_fx_0.bone;
    if (Rogue_IsAbilityState(GET_FIGHTER(gobj)))
        idx = Rogue_AbilityMapBone(GET_FIGHTER(gobj), idx);
    NEXT_CMD(cmd);
""",
    "remap script wind-effect bone",
)

path, text = load("src/melee/ft/ftaction.c")
if 'Rogue_DebugCheckpointEvent("script", eventCode);' not in text:
    block1 = """            eventCode =
                gmScriptEventCast(ftCommand->u, gmScriptEventDefault)->opcode;
"""
    if block1 not in text:
        raise SystemExit("script trace: primary loop anchor not found")
    text = text.replace(
        block1,
        block1 + '            Rogue_DebugCheckpointEvent("script", eventCode);\n',
        1,
    )
    block2 = """                eventCode =
                    gmScriptEventCast(cmd->u, gmScriptEventDefault)->opcode;
"""
    n = text.count(block2)
    if n < 1:
        raise SystemExit(f"script trace: expected at least one secondary loop, found {n}")
    text = text.replace(
        block2,
        block2 + '                Rogue_DebugCheckpointEvent("script", eventCode);\n',
        n,
    )
    save(path, text)
    print("round2: trace fighter-script opcodes")

print("round2: postpatch applied")
