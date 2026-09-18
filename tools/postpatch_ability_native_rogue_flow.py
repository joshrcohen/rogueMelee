"""Native Rogue menu/game-over compatibility for clean clones."""
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
# Progression menu:
# GS_TOU_BRACKET is a real non-gameplay Melee menu scene. For GM_ROGUE,
# bypass tournament state logic and let Rogue own only the frame/input loop.
# Tournament OnEnter/OnExit still provide native menu resources/SIS lifecycle.
# ---------------------------------------------------------------------------
path = root / "src" / "melee" / "gm" / "gmtou_1.c"
text = path.read_text(encoding="utf-8")

include = '#include <melee/rogue/rogue.h>\n'
if include not in text:
    # Exact include present in pinned 11749c9.
    anchor = '#include <melee/mn/mnmain.h>\n'
    if anchor not in text:
        # Secondary fallback in case nearby includes are reorganized later.
        anchor = '#include <melee/mn/inlines.h>\n'
    if anchor not in text:
        raise SystemExit("native Rogue flow: gmtou_1 include anchor changed")
    text = text.replace(anchor, anchor + include, 1)

old = """    PAD_STACK(4);

    data = gm_GetTournamentData();"""
new = """    PAD_STACK(4);

    if (gm_GetCurrentGameMode() == GM_ROGUE) {
        Rogue_RouteMenuSceneFrame();
        return;
    }

    data = gm_GetTournamentData();"""
if new not in text:
    if old not in text:
        raise SystemExit("native Rogue flow: tournament frame hook changed")
    text = text.replace(old, new, 1)


# Rogue must bypass Tournament OnEnter/OnExit as well as OnFrame.
old = """void gm_Scene_TouBracket_OnEnter(void* arg0)
{
    lbl_804D6668 = NULL;"""
new = """void gm_Scene_TouBracket_OnEnter(void* arg0)
{
    if (gm_GetCurrentGameMode() == GM_ROGUE) {
        /*
         * Rogue uses this only as a non-gameplay menu host.
         * Do not initialize Tournament bracket/model state.
         * SIS creates the ortho camera RogueUI needs.
         */
        HSD_SisLib_803A62A0(0, fn_8018F5F0(), "SIS_TournamentData");
        return;
    }

    lbl_804D6668 = NULL;"""
if new not in text:
    if old not in text:
        raise SystemExit("native Rogue flow: tournament enter hook changed")
    text = text.replace(old, new, 1)

old = """void gm_Scene_TouBracket_OnExit(void* arg0)
{
    lbArchive_80016EFC(lbl_804D6660);"""
new = """void gm_Scene_TouBracket_OnExit(void* arg0)
{
    if (gm_GetCurrentGameMode() == GM_ROGUE) {
        /*
         * Rogue did not load Tournament's model archives above.
         * Release only the SIS slot it created.
         */
        HSD_SisLib_803A5F50(0);
        return;
    }

    lbArchive_80016EFC(lbl_804D6660);"""
if new not in text:
    if old not in text:
        raise SystemExit("native Rogue flow: tournament exit hook changed")
    text = text.replace(old, new, 1)


path.write_text(text, encoding="utf-8")


# ---------------------------------------------------------------------------
# Progression matchup:
# State 4 intentionally stays on GS_TOU_BRACKET. Do not hook GmIntEz here:
# entering a second Classic fighter-presentation scene directly from the CSS
# caused the initial character-confirm crash. The ordinary state-1 Classic
# intro remains untouched.
# ---------------------------------------------------------------------------

# ---------------------------------------------------------------------------
# Wii 1.7 + -lang c99 compatibility:
# The pinned gm/types.h has two explicit file-scope STATIC_ASSERTs for
# TmSettingTable. Metrowerks Wii 1.7 rejects that anonymous-struct macro form
# in C99 mode. Use the project's ASSERT_OFFSET form instead; it preserves the
# intended check in matching/lint builds and compiles away in this nonmatching
# Rogue build.
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

print("postpatch: Rogue progression uses crash-safe Tournament/SIS host")
print("postpatch: Rogue Game Over uses native Classic character assets")
