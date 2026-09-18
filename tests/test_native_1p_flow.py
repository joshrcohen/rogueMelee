import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class NativeOnePlayerFlowTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.rogue = (ROOT / "mod/rogue.c").read_text(encoding="utf-8")
        cls.state_h = (ROOT / "mod/rogue_state.h").read_text(encoding="utf-8")
        cls.encounter = (ROOT / "mod/rogue_encounter.c").read_text(encoding="utf-8")
        cls.ui = (ROOT / "mod/rogue_ui.c").read_text(encoding="utf-8")
        cls.build = (ROOT / "tools/build.py").read_text(encoding="utf-8")
        cls.fixer = (
            ROOT / "tools/postpatch_ability_native_rogue_flow.py"
        ).read_text(encoding="utf-8")

    def test_real_classic_css_starts_run(self):
        self.assertIn("gm_801B06B0(css, REG_CLASSIC", self.rogue)
        self.assertIn(
            "gm_801B0730(css, &kind, &stocks, &costume, &nametag, &difficulty)",
            self.rogue,
        )

    def test_native_css_settings_are_persisted(self):
        self.assertIn("u8 difficulty;", self.state_h)
        self.assertIn("u8 player_stocks;", self.state_h)
        self.assertIn("u8 continues;", self.state_h)

    def test_difficulty_changes_enemy_setup(self):
        self.assertIn("static void Rogue_ApplyDifficulty", self.encounter)

    def test_selected_stocks_drive_rogue_matches(self):
        self.assertIn("g_rogue_run.player_stocks", self.encounter)

    def test_vanilla_stage_clear_shell_is_enabled(self):
        self.assertIn("start->rules.x4_4 = true;", self.rogue)
        self.assertIn("start->rules.x18", self.rogue)

    def test_reward_picker_is_old_stage_clear_flow(self):
        post = self.rogue.split("bool Rogue_PostFight(void)", 1)[1]
        post = post.split("static void enterGameOver", 1)[0]
        self.assertIn("RogueUI_OpenResults();", post)
        self.assertNotIn("routeSceneStateForChoice", post)
        self.assertNotIn("Rogue_RouteMenuSceneFrame", self.rogue)

    def test_old_route_scene_is_restored(self):
        self.assertIn("static void enterRoute(GameModeState* state)", self.rogue)
        self.assertIn("static void exitRoute(GameModeState* state)", self.rogue)
        self.assertRegex(
            self.rogue,
            r"4,\s*lbDvdPreload_2,\s*0,\s*enterRoute,\s*exitRoute",
        )
        self.assertNotIn("GS_TOU_BRACKET, NULL, NULL", self.rogue)

    def test_postfight_hook_remains_in_engine_patch(self):
        patch = (ROOT / "patches/engine.patch").read_text(encoding="utf-8")
        self.assertIn("case 3:", patch)
        self.assertIn("if (!Rogue_PostFight()) gm_801A4B60();", patch)

    def test_no_custom_progression_scene_postpatch(self):
        self.assertNotIn("gmtou_1.c", self.fixer)
        self.assertNotIn("gm_1832.c", self.fixer)
        self.assertNotIn("Rogue_RouteMenuSceneFrame", self.fixer)

    def test_real_continue_screen_is_used(self):
        self.assertIn("static DebugGameOverData game_over_data;", self.rogue)
        self.assertIn(
            "{ GS_GAMEOVER, &game_over_data, &game_over_data }",
            self.rogue,
        )

    def test_game_over_uses_classic_assets(self):
        self.assertIn("case GM_ROGUE:", self.fixer)
        self.assertIn("game_mode = GM_CLASSIC;", self.fixer)

    def test_build_uses_tracked_fixer(self):
        self.assertIn("postpatch_ability_native_rogue_flow.py", self.build)

    def test_pinned_nametag_name(self):
        self.assertIn("GM_NAMETAG_NONE", self.rogue)
        self.assertNotIn("GM_NAMETAG_COUNT", self.rogue)


if __name__ == "__main__":
    unittest.main()
