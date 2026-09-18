#!/usr/bin/env python3
from pathlib import Path
import re
import sys

root = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else Path.cwd()

def load(rel):
    p = root / rel
    if not p.is_file():
        raise SystemExit(f"Missing expected file: {p}")
    return p, p.read_text(encoding="utf-8")

def save(p, s):
    p.write_text(s, encoding="utf-8", newline="\n")

def replace_once(rel, old, new, label, required=True):
    p, s = load(rel)
    if new in s:
        return False
    if old not in s:
        if required:
            raise SystemExit(f"{label}: expected source not found in {rel}")
        return False
    s = s.replace(old, new, 1)
    save(p, s)
    print("round7:", label)
    return True

def add_include(rel, anchor, inc, label):
    p, s = load(rel)
    if inc in s:
        return False
    if anchor not in s:
        raise SystemExit(f"{label}: include anchor not found in {rel}")
    s = s.replace(anchor, anchor + inc, 1)
    save(p, s)
    print("round7:", label)
    return True

# ---------------------------------------------------------------------------
# Build config: these newly modified native files must use the same compiler
# setup as the rest of RogueMelee's engine integration.
# ---------------------------------------------------------------------------
p, s = load("configure.py")
start = s.find("rogue_native_objects = {")
if start < 0:
    raise SystemExit("round7: rogue_native_objects set not found in configure.py")
end = s.find("\n}", start)
if end < 0:
    raise SystemExit("round7: end of rogue_native_objects set not found")
block = s[start:end]
need = []
for obj in (
    "melee/ft/ftlib.c",
    "melee/ft/kinds/ftGameWatch/ftgamewatch.c",
):
    line = f"    '{obj}',\n"
    if line not in block:
        need.append(line)
if need:
    insert_at = s.find("\n", start) + 1
    s = s[:insert_at] + "".join(need) + s[insert_at:]
    save(p, s)
    print("round7: register ftlib/GameWatch init as Rogue native objects")

# ---------------------------------------------------------------------------
# 1) GAME & WATCH ARTICLES
#
# G&W's common article initializer decides between Game & Watch and Kirby-copy
# setup from the physical fighter kind. Borrowed G&W specials must dispatch by
# the active source kind instead.
# ---------------------------------------------------------------------------
add_include(
    "src/melee/ft/ftlib.c",
    '#include "ftlib.h"\n',
    '#include <melee/rogue/rogue_ability.h>\n',
    "include borrowed ability support in ftlib",
)

p, s = load("src/melee/ft/ftlib.c")
old = "if (ftLib_GetKind(gobj) == Ft_Kind_GameWatch) {"
new = "if (Rogue_AbilitySourceKind(GET_FIGHTER(gobj)) == Ft_Kind_GameWatch) {"
if new not in s:
    n = s.count(old)
    if n < 2:
        raise SystemExit(
            f"round7: expected two G&W article dispatches in ftlib.c, found {n}"
        )
    s = s.replace(old, new, 2)
    save(p, s)
    print("round7: make G&W article setup source-aware")

# G&W articles index a four-entry color table by costume. A borrowed recipient
# can have costume IDs > 3, so use a deterministic G&W-compatible entry.
add_include(
    "src/melee/ft/kinds/ftGameWatch/ftgamewatch.c",
    '#include "ftgamewatch.h"\n',
    '#include <melee/rogue/rogue_ability.h>\n',
    "include borrowed ability support in GameWatch init",
)
replace_once(
    "src/melee/ft/kinds/ftGameWatch/ftgamewatch.c",
    """void ftGw_Init_8014A7F4(HSD_GObj* gobj, ItemModStruct* item_mod)
{
    Fighter* fp = GET_FIGHTER(gobj);
    ftGameWatchAttributes* gawAttrs = getFtSpecialAttrs(fp);

    item_mod->x0_unk = gawAttrs->x4_GAMEWATCH_COLOR[fp->x619_costume_id];
}
""",
    """void ftGw_Init_8014A7F4(HSD_GObj* gobj, ItemModStruct* item_mod)
{
    Fighter* fp = GET_FIGHTER(gobj);
    ftGameWatchAttributes* gawAttrs = getFtSpecialAttrs(fp);
    int costume = fp->x619_costume_id;

    if (Rogue_IsAbilityState(fp) && fp->kind != Ft_Kind_GameWatch)
        costume &= 3;

    item_mod->x0_unk = gawAttrs->x4_GAMEWATCH_COLOR[costume];
}
""",
    "make borrowed G&W article color safe",
)

# Fire/Rescue attaches its article to a raw G&W joint. The article system later
# indexes the recipient fp->parts[], so pass the mapped recipient joint.
replace_once(
    "src/melee/ft/kinds/ftGameWatch/ftgamewatchspecialhi.c",
    """        rescueGObj = it_802C8038(gobj, &sp10, FtPart_TopN,
""",
    """        rescueGObj = it_802C8038(
            gobj, &sp10, Rogue_AbilityMapBone(fp, FtPart_TopN),
""",
    "map G&W Fire/Rescue article attachment",
)

# ---------------------------------------------------------------------------
# 2) SOURCE HITBOX / HURTBOX / DISJOINT COLLISION BONES
#
# Centralize source->recipient mapping. This covers scripted hurt-state events
# and direct native helpers such as Bowser's Whirling Fortress without adding
# per-character hacks.
# ---------------------------------------------------------------------------

# Round 2 mapped scripted hurt bones at the call site. Revert that one caller
# to raw source IDs so the shared ftColl_8007B128 compatibility layer below is
# the single mapping point. This avoids double mapping.
p, s = load("src/melee/ft/ftaction.c")
round2_block = """void ftAction_80071A9C(Fighter_GObj* gobj, CommandInfo* cmd)
{
    Fighter* fp = GET_FIGHTER(gobj);
    int bone = cmd->u->set_hurt_state.bone_idx;
    if (Rogue_IsAbilityState(fp))
        bone = Rogue_AbilityMapBone(fp, bone);
    ftColl_8007B128(gobj, bone, cmd->u->set_hurt_state.state);
    NEXT_CMD(cmd);
}
"""
native_block = """void ftAction_80071A9C(Fighter_GObj* gobj, CommandInfo* cmd)
{
    ftColl_8007B128(gobj, cmd->u->set_hurt_state.bone_idx,
                    cmd->u->set_hurt_state.state);
    NEXT_CMD(cmd);
}
"""
if round2_block in s:
    s = s.replace(round2_block, native_block, 1)
    save(p, s)
    print("round7: centralize scripted hurt-capsule mapping")
elif native_block not in s:
    raise SystemExit("round7: could not normalize ftAction_80071A9C")

# If an earlier experimental Round 7 mapped Bowser's direct calls at the
# callsite, normalize those back to raw source IDs too.
p, s = load("src/melee/ft/kinds/ftKoopa/ftkoopaspecialn.c")
pattern = re.compile(
    r"ftColl_8007B128\(gobj,\s*"
    r"Rogue_AbilityMapBone\(GET_FIGHTER\(gobj\),\s*(0x[0-9A-Fa-f]+|\d+)\),\s*"
    r"HurtCapsule_Intangible\);"
)
s2, n = pattern.subn(
    r"ftColl_8007B128(gobj, \1, HurtCapsule_Intangible);", s
)
if n:
    s = s2
    save(p, s)
    print(f"round7: normalized {n} Bowser hurt-capsule callsites")

# Shared hurt-capsule state setter: source joint -> recipient joint. Some source
# body parts have no equivalent hurt capsule on the recipient; those are visual
# / anatomy differences, not fatal errors, so borrowed moves fail-soft there.
p, s = load("src/melee/ft/ftcoll.c")
old_head = """void ftColl_8007B128(Fighter_GObj* fighter_gobj, int bone_id,
                     HurtCapsuleState state)
{
    Fighter* fp = GET_FIGHTER(fighter_gobj);
    int i;
"""
new_head = """void ftColl_8007B128(Fighter_GObj* fighter_gobj, int bone_id,
                     HurtCapsuleState state)
{
    Fighter* fp = GET_FIGHTER(fighter_gobj);
    int i;

    if (Rogue_IsAbilityState(fp))
        bone_id = Rogue_AbilityMapBone(fp, bone_id);
"""
if new_head not in s:
    if old_head not in s:
        raise SystemExit("round7: ftColl_8007B128 function header not found")
    s = s.replace(old_head, new_head, 1)

old_tail = """    HSD_ASSERTREPORT(0x888, 0,
                     "in ftCollisionSetHitStatus illegal parts!\\n");
}
"""
new_tail = """    if (Rogue_IsAbilityState(fp)) {
        /* The source fighter can have anatomy the recipient simply lacks. */
        return;
    }

    HSD_ASSERTREPORT(0x888, 0,
                     "in ftCollisionSetHitStatus illegal parts!\\n");
}
"""
if new_tail not in s:
    if old_tail not in s:
        raise SystemExit("round7: ftColl_8007B128 assert tail not found")
    # The first occurrence after our function header is the one we want.
    pos = s.find(new_head if new_head in s else old_head)
    tail_pos = s.find(old_tail, pos)
    if tail_pos < 0:
        raise SystemExit("round7: ftColl_8007B128 assert tail not found after function")
    s = s[:tail_pos] + new_tail + s[tail_pos + len(old_tail):]

# Shield / counter / Toad-style collision descriptors also carry source bones.
old = "    fp->shield_hit.bone = fp->parts[shield->bone].joint;"
new = "    fp->shield_hit.bone = fp->parts[Rogue_AbilityMapBone(fp, shield->bone)].joint;"
if new not in s:
    if old not in s:
        raise SystemExit("round7: shield descriptor bone assignment not found")
    s = s.replace(old, new, 1)

# Reflect / absorb descriptors are already mapped by engine.patch. Normalize if
# a future checkout ever lacks those earlier changes.
for old, new, label in (
    (
        "    fp->reflect_hit.bone = fp->parts[reflect->x0_bone_id].joint;",
        "    fp->reflect_hit.bone = fp->parts[Rogue_AbilityMapBone(fp, reflect->x0_bone_id)].joint;",
        "reflect",
    ),
    (
        "    fp->absorb_hit.bone = fp->parts[absorb->x0_bone_id].joint;",
        "    fp->absorb_hit.bone = fp->parts[Rogue_AbilityMapBone(fp, absorb->x0_bone_id)].joint;",
        "absorb",
    ),
):
    if new not in s and old in s:
        s = s.replace(old, new, 1)
        print(f"round7: map {label} descriptor bone")

save(p, s)
print("round7: generic borrowed collision-bone compatibility installed")

# Verify raw fighter hitbox creation is still source->recipient mapped.
_, s = load("src/melee/ft/ftaction.c")
if "hitbox->jobj = fp->parts[Rogue_AbilityMapBone(fp, idx)].joint;" not in s:
    raise SystemExit(
        "round7: raw hitbox creation is not mapped; expected engine.patch integration missing"
    )

# ---------------------------------------------------------------------------
# 3) HELD / ATTACHED ARTICLES
#
# Item attachment APIs expect a JOINT INDEX ON THE RECIPIENT. Do not globally
# remap inside Item_8026AB54 because several callers already pass recipient
# joints (for example G&W Judgment). Fix source callsites that supplied source
# metadata instead.
# ---------------------------------------------------------------------------

# Link/Young Link Bomb:
# - da->x48 is the bomb item kind, not a skeleton bone. Round 1 mistakenly
#   passed it through Rogue_AbilityMapBone().
# - x8->x10 is the fighter's held-item joint and must come from the recipient.
p, s = load("src/melee/ft/kinds/ftLink/ftlinkspeciallw.c")
bad_kind = "Fighter_Part part = Rogue_AbilityMapBone(fp, da->x48);"
good_kind = "Fighter_Part part = da->x48;"
if bad_kind in s:
    s = s.replace(bad_kind, good_kind, 1)
    print("round7: restore Link/Young Link Bomb item kind")
elif good_kind not in s:
    raise SystemExit("round7: Link Bomb item-kind assignment not found")

for expr in (
    "Rogue_AbilityMapBone(fp, Rogue_AbilityData(fp)->x8->x10)",
    "Rogue_AbilityData(fp)->x8->x10",
):
    if expr in s:
        s = s.replace(expr, "fp->ft_data->x8->x10", 1)
        print("round7: use recipient held-item joint for Link/Young Link Bomb")
        break
if "fp->ft_data->x8->x10" not in s:
    raise SystemExit("round7: Link Bomb recipient held-item joint not present")
save(p, s)

# Peach Vegetable uses the same held-item rule.
p, s = load("src/melee/ft/kinds/ftPeach/ftpeachspeciallw.c")
changed = False
for expr in (
    "Rogue_AbilityMapBone(fp, Rogue_AbilityData(fp)->x8->x10)",
    "Rogue_AbilityData(fp)->x8->x10",
):
    if expr in s:
        s = s.replace(expr, "fp->ft_data->x8->x10", 1)
        changed = True
        break
if changed:
    save(p, s)
    print("round7: use recipient held-item joint for Peach Vegetable")
elif "fp->ft_data->x8->x10" not in s:
    raise SystemExit("round7: Peach Vegetable held-item joint expression not found")

# Peach Toad and Parasol use Peach's raw source joint 109 for their attached
# articles. Ensure both article attachment calls use the recipient-mapped joint.
p, s = load("src/melee/ft/kinds/ftPeach/ftpeachspecialn.c")
old = "it_802BDE18(gobj, &pos, FtPart_109,"
new = "it_802BDE18(gobj, &pos, Rogue_AbilityMapBone(fp, FtPart_109),"
if new not in s:
    if old not in s:
        raise SystemExit("round7: Peach Toad attachment call not found")
    s = s.replace(old, new, 1)
    save(p, s)
    print("round7: map Peach Toad article attachment")

p, s = load("src/melee/ft/kinds/ftPeach/ftpeachspecialhi.c")
old = "it_802BDA64(gobj, &pos, FtPart_109, fp->facing_dir)"
new = (
    "it_802BDA64(gobj, &pos, Rogue_AbilityMapBone(fp, FtPart_109), "
    "fp->facing_dir)"
)
if new not in s:
    if old not in s:
        raise SystemExit("round7: Peach Parasol attachment call not found")
    s = s.replace(old, new, 1)
    save(p, s)
    print("round7: map Peach Parasol article attachment")

# Parasol subaction event 0x2A can request article animation before/after an
# attached visual exists. Native Melee asserts here. For a borrowed move, a
# missing source-only visual must not kill the fighter update.
p, s = load("src/melee/ft/ftcommon.c")
old = """    HSD_ASSERT(1276, ftGetParasolStatus(gobj) != FtParasol_None);
"""
new = """    if (Rogue_IsAbilityState(fp) &&
        ftGetParasolStatus(gobj) == FtParasol_None)
    {
        return;
    }

    HSD_ASSERT(1276, ftGetParasolStatus(gobj) != FtParasol_None);
"""
if new not in s:
    if old not in s:
        raise SystemExit("round7: parasol status assert not found")
    s = s.replace(old, new, 1)
    save(p, s)
    print("round7: make borrowed Parasol article animation fail-soft")

print("round7: systemic source-wide hitbox/article compatibility fixes applied")
