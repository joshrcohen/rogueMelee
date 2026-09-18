import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class NativeOnePlayerUiTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.ui = (ROOT / "mod/rogue_ui.c").read_text(encoding="utf-8")
        cls.rogue = (ROOT / "mod/rogue.c").read_text(encoding="utf-8")

    def test_combined_stage_clear_progression_popup(self):
        section = self.ui.split("static void drawStageClear(void)", 1)[1]
        section = section.split("static void drawRunEnd(void)", 1)[0]
        self.assertIn('"ROGUE PROGRESSION"', section)
        self.assertIn('"ROUTE   1  >  2  >  ELITE  >  4  >  SHOP  >  BOSS"', section)
        self.assertIn('"CHOOSE UPGRADE"', section)
        self.assertIn('"CHOOSE NEXT FIGHT"', section)
        self.assertIn('"RUN STATS"', section)

    def test_stage_clear_renderer_is_lightweight(self):
        section = self.ui.split("static void drawStageClear(void)", 1)[1]
        section = section.split("static void drawRunEnd(void)", 1)[0]
        self.assertNotIn("ui_panel_box(", section)
        self.assertNotIn("ui_backdrop(", section)
        self.assertNotIn("ui_fighter_icon(", section)
        self.assertNotIn("stageProgressNode", section)
        self.assertNotIn("stageRewardBox", section)
        self.assertNotIn("stageFightBox", section)

    def test_stage_clear_has_three_upgrade_entries(self):
        section = self.ui.split("static void drawStageClear(void)", 1)[1]
        section = section.split("static void drawRunEnd(void)", 1)[0]
        self.assertIn("for (i = 0; i < 3; ++i)", section)
        self.assertIn("g_rogue_run.current_rewards[i]", section)
        self.assertIn("Rogue_DescribeReward", section)

    def test_native_classic_matchup_intro(self):
        self.assertIn("GS_INTRO_EASY, &stage_intro", self.rogue)

    def test_initial_route_screen_still_exists(self):
        self.assertIn('"ROGUE ROUTE"', self.ui)
        self.assertIn('"CHOOSE NEXT MATCH"', self.ui)

    def test_native_continue_screen(self):
        self.assertIn("GS_GAMEOVER, &game_over_data, &game_over_data", self.rogue)

    def test_hud_is_simplified(self):
        self.assertIn('"ACT %d-%d   G%d   %s"', self.ui)


if __name__ == "__main__":
    unittest.main()
