import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class NativeOnePlayerFlowTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.rogue = (ROOT / "mod/rogue.c").read_text(encoding="utf-8")
        cls.progress = (ROOT / "mod/rogue_progression.c").read_text(encoding="utf-8")
        cls.state_h = (ROOT / "mod/rogue_state.h").read_text(encoding="utf-8")
        cls.encounter = (ROOT / "mod/rogue_encounter.c").read_text(encoding="utf-8")
        cls.fixer = (
            ROOT / "tools/postpatch_ability_native_rogue_flow.py"
        ).read_text(encoding="utf-8")

    def test_real_classic_css_starts_run(self):
        self.assertIn("gm_801B06B0(css, REG_CLASSIC", self.rogue)

    def test_native_css_settings_are_persisted(self):
        self.assertIn("u8 difficulty;", self.state_h)
        self.assertIn("u8 player_stocks;", self.state_h)
        self.assertIn("u8 continues;", self.state_h)

    def test_difficulty_changes_enemy_setup(self):
        self.assertIn("static void Rogue_ApplyDifficulty", self.encounter)

    def test_selected_stocks_drive_rogue_matches(self):
        self.assertIn("g_rogue_run.player_stocks", self.encounter)

    def test_native_stage_clear_remains_enabled(self):
        self.assertIn("start->rules.x4_4 = true;", self.rogue)
        self.assertIn("start->rules.x18", self.rogue)

    def test_css_goes_to_progression_not_rest_area(self):
        section = self.rogue.split("static void exitCharacterSelect", 1)[1]
        section = section.split("static void enterStageIntro", 1)[0]
        self.assertIn("ROGUE_STATE_PROGRESSION", section)
        self.assertNotIn("gm_SetNextGameModeStateId(4);", section)

    def test_progression_is_native_classic_intro(self):
        self.assertRegex(
            self.rogue,
            r"ROGUE_STATE_PROGRESSION,\s*lbDvdPreload_3,\s*0,\s*"
            r"RogueProgression_Enter,\s*RogueProgression_Exit,\s*"
            r"\{\s*GS_INTRO_EASY,\s*&g_rogue_progression_intro,\s*NULL\s*\}",
        )

    def test_reward_postfight_exits_stage_clear_to_progression(self):
        post = self.rogue.split("bool Rogue_PostFight(void)", 1)[1]
        post = post.split("static void enterGameOver", 1)[0]
        reward = post.split("if (g_rogue_run.phase == ROGUE_PHASE_REWARD)", 1)[1]
        self.assertIn("destination = ROGUE_STATE_PROGRESSION;", reward)
        self.assertIn("return false;", reward)

    def test_progression_uses_two_step_upgrade_then_fight(self):
        reward_pos = self.progress.index("Rogue_SelectReward(chosen)")
        fight_pos = self.progress.index("RogueRoute_Select(&g_rogue_run.route,")
        self.assertLess(reward_pos, fight_pos)

    def test_real_matchup_intro_still_follows_choice(self):
        self.assertIn("GS_INTRO_EASY, &stage_intro", self.rogue)
        self.assertIn("gm_SetNextGameModeStateId(next_intro_state());", self.progress)

    def test_introeasy_hook_is_narrow(self):
        self.assertIn("if (Rogue_ProgressionIntroFrame())", self.fixer)

    def test_real_continue_screen_is_used(self):
        self.assertIn("GS_GAMEOVER, &game_over_data, &game_over_data", self.rogue)


if __name__ == "__main__":
    unittest.main()
