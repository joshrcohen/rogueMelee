"""Compatibility postpatch for the pre-combined Rogue flow.

This intentionally contains NO Tournament or Classic-intro progression hooks.
"""
from pathlib import Path
import sys

root = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else Path.cwd()

# ---------------------------------------------------------------------------
# Game Over: Rogue reuses Classic character/model resources.
# ---------------------------------------------------------------------------
path = root / "src" / "melee" / "gm" / "gm_19EF.c"
text = path.read_text(encoding="utf-8")

old = """    switch (gm_GetCurrentGameMode()) {
    case GM_CLASSIC:
        return gm_80160474(arg0, GM_CLASSIC);
    case GM_ADVENTURE:"""
new = """    switch (gm_GetCurrentGameMode()) {
    case GM_CLASSIC:
    case GM_ROGUE:
        return gm_80160474(arg0, GM_CLASSIC);
    case GM_ADVENTURE:"""
if new not in text:
    if old not in text:
        raise SystemExit("native Rogue flow: game-over character mapping changed")
    text = text.replace(old, new, 1)

old = """static inline void fn_8019F9C4_LoadSymbols(u32 arg0)
{
    u8 game_mode = gm_GetCurrentGameMode();
    char* model_name = gm_80160564(arg0, game_mode);
    char* scene_name = gm_801604DC(arg0, game_mode);

    lbArchive_LoadSymbols(scene_name, &lbl_804D66AC, model_name, 0);"""
new = """static inline void fn_8019F9C4_LoadSymbols(u32 arg0)
{
    u8 game_mode = gm_GetCurrentGameMode();
    char* model_name;
    char* scene_name;

    if (game_mode == GM_ROGUE)
        game_mode = GM_CLASSIC;

    model_name = gm_80160564(arg0, game_mode);
    scene_name = gm_801604DC(arg0, game_mode);

    lbArchive_LoadSymbols(scene_name, &lbl_804D66AC, model_name, 0);"""
if new not in text:
    if old not in text:
        raise SystemExit("native Rogue flow: game-over symbol mapping changed")
    text = text.replace(old, new, 1)

path.write_text(text, encoding="utf-8")


# ---------------------------------------------------------------------------
# Rogue progression: hold only Rogue state 7's retail Classic intro scene.
# ---------------------------------------------------------------------------
path = root / "src" / "melee" / "gm" / "gm_1832.c"
text = path.read_text(encoding="utf-8")

old = '#include "gmscene.h"\n#include <melee/cm/camera.h>'
new = '#include "gmscene.h"\n#include <melee/rogue/rogue_progression.h>\n#include <melee/cm/camera.h>'
if new not in text:
    if old not in text:
        raise SystemExit("native Rogue flow: gm_1832 include anchor changed")
    text = text.replace(old, new, 1)

# ---------------------------------------------------------------------------
# Rogue state 7 freezes the retail IntroEasy timeline before NOW LOADING.
#
# gm_Scene_IntroEasy_OnFrame is already held by Rogue input, but fn_80184AB8
# is a separate GObj proc and otherwise continues advancing the retail intro
# model until its terminal/loading animation. State 7 owns its own lifetime,
# so stop only that presentation timeline after the VS composition is ready.
# The native IrRdMap has a separate proc (fn_8018504C) and keeps animating.
# ---------------------------------------------------------------------------
old = """    jobj = arg0->hsd_obj;
    HSD_JObjAnimAll(jobj);

    if (lbl_804735A8.x38 < 0x8CU) {"""

new = """    jobj = arg0->hsd_obj;

    if (gm_GetCurrentGameMode() == GM_ROGUE &&
        gm_GetCurrentSceneIndex() == ROGUE_STATE_PROGRESSION &&
        lbl_804735A8.x38 >= 0x63U)
    {
        return;
    }

    HSD_JObjAnimAll(jobj);

    if (lbl_804735A8.x38 < 0x8CU) {"""

if new not in text:
    if old not in text:
        raise SystemExit("native Rogue flow: IntroEasy timeline anchor changed")
    text = text.replace(old, new, 1)

old = (
    'void gm_Scene_IntroEasy_OnFrame(void)\n'
    '{\n'
    '    if (lbl_804735A8.x0 != 0) {\n'
    '        lbAudioAx_800236DC();\n'
    '        gm_801A4B60();\n'
    '    }\n'
    '}\n'
)
new = (
    'void gm_Scene_IntroEasy_OnFrame(void)\n'
    '{\n'
    '    /* Rogue state 7 holds the real Classic VS presentation for input. */\n'
    '    if (Rogue_ProgressionIntroFrame())\n'
    '        return;\n'
    '\n'
    '    if (lbl_804735A8.x0 != 0) {\n'
    '        lbAudioAx_800236DC();\n'
    '        gm_801A4B60();\n'
    '    }\n'
    '}\n'
)
if new not in text:
    if old not in text:
        raise SystemExit("native Rogue flow: IntroEasy frame anchor changed")
    text = text.replace(old, new, 1)

# ---------------------------------------------------------------------------
# Rogue progression: keep the retail route-map model untouched.
#
# The pinned IntroEasy scene already loads and animates the native road-map
# assets. Rogue V10 intentionally adds no JObj visibility/transform patch.
# ---------------------------------------------------------------------------
path.write_text(text, encoding="utf-8")

# ---------------------------------------------------------------------------
# Rogue progression: reward UI needs more SIS headroom than retail IntroEasy.
#
# Retail allocates only 0x4800 bytes for ordinary GameScenes, including
# GS_INTRO_EASY. The Rogue progression scene layers route/fight/build/reward
# text onto the retail Classic presentation. Give only Rogue state 7 the same
# 0xC000 SIS arena that retail already uses for GS_RESULTS.
# ---------------------------------------------------------------------------
path = root / "src" / "melee" / "gm" / "gm_1A3F.c"
text = path.read_text(encoding="utf-8")

old = """    switch (state->info.scene_kind) {
    case GS_STAFFROLL:
    case GS_RESULTS:
        HSD_SisLib_803A6048(0xC000);
        break;
    case GS_CSS:
        HSD_SisLib_803A6048(0x2400);
        break;
    default:
        HSD_SisLib_803A6048(0x4800);
        break;
    }"""

new = """    switch (state->info.scene_kind) {
    case GS_STAFFROLL:
    case GS_RESULTS:
        HSD_SisLib_803A6048(0xC000);
        break;
    case GS_CSS:
        HSD_SisLib_803A6048(0x2400);
        break;
    default:
        if (gm_GetCurrentGameMode() == GM_ROGUE && state->id == 7)
            HSD_SisLib_803A6048(0xC000);
        else
            HSD_SisLib_803A6048(0x4800);
        break;
    }"""

if new not in text:
    if old not in text:
        raise SystemExit(
            "native Rogue flow: progression SIS arena anchor changed"
        )
    text = text.replace(old, new, 1)

path.write_text(text, encoding="utf-8")

# ---------------------------------------------------------------------------
# Wii 1.7 + -lang c99 compatibility for modified vanilla translation units.
# ---------------------------------------------------------------------------
path = root / "src" / "melee" / "gm" / "types.h"
text = path.read_text(encoding="utf-8")

replacements = {
    "STATIC_ASSERT(offsetof(struct TmSettingTable, min) == 0x40);":
        "ASSERT_OFFSET(struct TmSettingTable, min, 0x40);",
    "STATIC_ASSERT(offsetof(struct TmSettingTable, max) == 0x4C);":
        "ASSERT_OFFSET(struct TmSettingTable, max, 0x4C);",
}

for old, new in replacements.items():
    if new not in text:
        if old not in text:
            raise SystemExit(
                "native Rogue flow: TmSettingTable assert layout changed"
            )
        text = text.replace(old, new, 1)

path.write_text(text, encoding="utf-8")

print("postpatch: pre-combined Rogue flow compatibility applied")

print("postpatch: native retail route map left untouched for Rogue progression")
