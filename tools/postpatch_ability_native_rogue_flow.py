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
print("postpatch: no Tournament/Classic-intro progression hooks installed")
