#!/usr/bin/env python3
from pathlib import Path
import sys

root = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else Path.cwd()

def load(rel):
    p = root / rel
    if not p.is_file():
        raise SystemExit(f"Missing expected file: {p}")
    return p, p.read_text(encoding="utf-8")

def save(p, s):
    p.write_text(s, encoding="utf-8", newline="\n")

def repl(rel, old, new, label):
    p, s = load(rel)

    # engine.patch already remaps raw hitbox joints with Rogue_AbilityMapBone().
    # Treat that equivalent implementation as already fixed instead of failing.
    if (label == "remap raw source hitbox joints" and
        "hitbox->jobj = fp->parts[Rogue_AbilityMapBone(fp, idx)].joint;" in s):
        print("round5: raw source hitbox joints already remapped by engine.patch")
        return

    if new in s:
        return
    if old not in s:
        raise SystemExit(f"{label}: expected source not found in {rel}")
    s = s.replace(old, new, 1)
    save(p, s)
    print("round5:", label)

# ---------------------------------------------------------------------------
# 1. Raw source-joint hitboxes must be remapped to the recipient skeleton.
#    Common-bone hitboxes already go through ftParts_GetBoneIndex(), which
#    Round 4 made safe.
# ---------------------------------------------------------------------------
repl(
    "src/melee/ft/ftaction.c",
    '''        idx = cmd->u->create_hitbox_0.bone;
        if (cmd->u->create_hitbox_0.use_common_bone_ids) {
            hitbox->jobj = fp->parts[ftParts_GetBoneIndex(
                                         fp, cmd->u->create_hitbox_0.bone)]
                               .joint;
        } else {
            hitbox->jobj = fp->parts[idx].joint;
        }
''',
    '''        idx = cmd->u->create_hitbox_0.bone;
        if (cmd->u->create_hitbox_0.use_common_bone_ids) {
            hitbox->jobj = fp->parts[ftParts_GetBoneIndex(
                                         fp, cmd->u->create_hitbox_0.bone)]
                               .joint;
        } else {
            if (Rogue_IsAbilityState(fp))
                idx = Rogue_AbilityMapBone(fp, idx);
            hitbox->jobj = fp->parts[idx].joint;
        }
''',
    "remap raw source hitbox joints",
)

# ---------------------------------------------------------------------------
# 2. GFX script events may also carry raw source joint indices. Convert them
#    once before ftCo_8009F834() so that function only sees recipient indices.
#    Preserve 0x8D/0x8E special sentinel values.
# ---------------------------------------------------------------------------
repl(
    "src/melee/ft/ftaction.c",
    '''            NEXT_CMD(cmd);
            ftCo_8009F834(gobj, gfx_id, bone, use_common_bone_id,
                          destroy_on_state_change, &offset, &range, unk);
''',
    '''            NEXT_CMD(cmd);
            if (Rogue_IsAbilityState(fp) && bone < 0x8D) {
                if (use_common_bone_id)
                    bone = ftParts_GetBoneIndex(fp, bone);
                else
                    bone = Rogue_AbilityMapBone(fp, bone);
                use_common_bone_id = 0;
            }
            ftCo_8009F834(gobj, gfx_id, bone, use_common_bone_id,
                          destroy_on_state_change, &offset, &range, unk);
''',
    "remap raw source GFX joints",
)

# ---------------------------------------------------------------------------
# 3. Part-animation commands are character-model-specific. Marth/Roy use them
#    for sword/model subanimations. Applying their ft_data->x1C definitions to
#    Falcon/DK/Fox/etc. later makes ftAnim_800707B0 walk incompatible recipient
#    part data. On a different recipient there is no source sword/model part to
#    animate, so skip only the visual part-animation command.
# ---------------------------------------------------------------------------
repl(
    "src/melee/ft/ftaction.c",
    '''void ftAction_800727C8(Fighter_GObj* gobj, CommandInfo* cmd)
{
    ftAnim_ApplyPartAnim(gobj, cmd->u->part_anim.unk1, cmd->u->part_anim.unk2,
                         cmd->u->part_anim.unk3);
    NEXT_CMD(cmd);
}
''',
    '''void ftAction_800727C8(Fighter_GObj* gobj, CommandInfo* cmd)
{
    Fighter* fp = GET_FIGHTER(gobj);
    if (!Rogue_IsAbilityState(fp) ||
        Rogue_AbilitySourceKind(fp) == fp->kind)
    {
        ftAnim_ApplyPartAnim(gobj, cmd->u->part_anim.unk1,
                             cmd->u->part_anim.unk2,
                             cmd->u->part_anim.unk3);
    } else {
        Rogue_DebugCheckpoint("part anim skipped");
    }
    NEXT_CMD(cmd);
}
''',
    "skip incompatible source part-animation command",
)

repl(
    "src/melee/ft/ftaction.c",
    '''void ftAction_8007283C(Fighter_GObj* gobj, CommandInfo* cmd)
{
    ftAnim_ApplyPartAnim(gobj, cmd->u->part_anim.unk1, cmd->u->part_anim.unk2,
                         0.0f);
    NEXT_CMD(cmd);
}
''',
    '''void ftAction_8007283C(Fighter_GObj* gobj, CommandInfo* cmd)
{
    Fighter* fp = GET_FIGHTER(gobj);
    if (!Rogue_IsAbilityState(fp) ||
        Rogue_AbilitySourceKind(fp) == fp->kind)
    {
        ftAnim_ApplyPartAnim(gobj, cmd->u->part_anim.unk1,
                             cmd->u->part_anim.unk2, 0.0f);
    } else {
        Rogue_DebugCheckpoint("part anim skipped");
    }
    NEXT_CMD(cmd);
}
''',
    "skip incompatible zero-blend source part-animation command",
)

# ---------------------------------------------------------------------------
# ftanim.c needs the QA checkpoint prototype before we add calls below.
# ---------------------------------------------------------------------------
p, s = load("src/melee/ft/ftanim.c")
debug_inc = "#include <melee/rogue/rogue_debug.h>\n"
if debug_inc not in s:
    anchor = "#include <melee/rogue/rogue_ability.h>\n"
    if anchor not in s:
        raise SystemExit("round5: ftanim.c rogue_ability include anchor not found")
    s = s.replace(anchor, anchor + debug_inc, 1)
    save(p, s)
    print("round5: include QA checkpoint prototype in ftanim.c")

# ---------------------------------------------------------------------------
# 4. Add post-script checkpoints. The previous result ended at script 0x02,
#    which is only an asynchronous timer. These checkpoints distinguish the
#    code immediately after script processing.
# ---------------------------------------------------------------------------
repl(
    "src/melee/ft/ftanim.c",
    '''void ftAnim_8006EBA4(Fighter_GObj* gobj)
{
    ftAnim_8006E9B4(gobj);
    ftAction_80073240(gobj);
    ftAnim_800707B0(gobj);
    ftCo_800DB500(gobj);
}
''',
    '''void ftAnim_8006EBA4(Fighter_GObj* gobj)
{
    ftAnim_8006E9B4(gobj);
    Rogue_DebugCheckpoint("anim advanced");
    ftAction_80073240(gobj);
    Rogue_DebugCheckpoint("script done");
    ftAnim_800707B0(gobj);
    Rogue_DebugCheckpoint("part anim done");
    ftCo_800DB500(gobj);
    Rogue_DebugCheckpoint("anim post done");
}
''',
    "trace post-script animation stages",
)

print("round5: postpatch applied")
