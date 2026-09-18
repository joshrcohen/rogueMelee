import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class NativeOnePlayerFlowTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.rogue = (ROOT / "mod/rogue.c").read_text(encoding="utf-8")
        cls.state_h = (ROOT / "mod/rogue_state.h").read_text(encoding="utf-8")
        cls.state_c = (ROOT / "mod/rogue_state.c").read_text(encoding="utf-8")
        cls.encounter = (ROOT / "mod/rogue_encounter.c").read_text(encoding="utf-8")
        cls.ui = (ROOT / "mod/rogue_ui.c").read_text(encoding="utf-8")
        cls.build = (ROOT / "tools/build.py").read_text(encoding="utf-8")
        cls.fixer = (ROOT / "tools/postpatch_native_1p_flow.py").read_text(
            encoding="utf-8"
        )

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
        self.assertIn("attack *= 1.16f;", self.encounter)
        self.assertIn("defense *= 1.10f;", self.encounter)

    def test_selected_stocks_drive_rogue_matches(self):
        self.assertIn("g_rogue_run.player_stocks", self.encounter)

    def test_real_stage_clear_shell_is_enabled(self):
        self.assertIn("start->rules.x4_4 = true;", self.rogue)
        self.assertIn('"CHOOSE UPGRADE"', self.ui)
        self.assertIn("overlay_sis = 0;", self.ui)
        self.assertIn("overlay_canvas = 0;", self.ui)
        self.assertIn("RogueUI_Clear();", self.ui)

    def test_reward_selection_happens_inside_regclear(self):
        self.assertIn("case 2:", self.fixer)
        self.assertIn("gm_GetCurrentGameMode() == GM_ROGUE", self.fixer)
        self.assertIn("Rogue_PostFight()", self.fixer)

    def test_real_continue_screen_is_used(self):
        self.assertIn("static DebugGameOverData game_over_data;", self.rogue)
        self.assertIn(
            "{ GS_GAMEOVER, &game_over_data, &game_over_data }",
            self.rogue,
        )

    def test_continue_retries_current_encounter(self):
        self.assertIn("--g_rogue_run.continues;", self.rogue)
        self.assertIn(
            "g_rogue_run.phase = ROGUE_PHASE_ENCOUNTER;",
            self.rogue,
        )

    def test_game_over_scene_uses_classic_assets_for_rogue(self):
        self.assertIn("case GM_ROGUE:", self.fixer)
        self.assertIn("game_mode = GM_CLASSIC;", self.fixer)

    def test_build_runs_native_flow_postpatch(self):
        self.assertIn("postpatch_native_1p_flow.py", self.build)

    def test_pinned_nametag_name(self):
        self.assertIn("GM_NAMETAG_NONE", self.rogue)
        self.assertNotIn("GM_NAMETAG_COUNT", self.rogue)


if __name__ == "__main__":
    unittest.main()
