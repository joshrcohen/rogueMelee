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
        self.assertIn('"ROUTE"', section)
        self.assertIn('"CHOOSE UPGRADE"', section)
        self.assertIn('"CHOOSE NEXT FIGHT"', section)
        self.assertIn('"RUN STATS"', section)

    def test_stage_clear_has_three_upgrades(self):
        section = self.ui.split("static void drawStageClear(void)", 1)[1]
        section = section.split("static void drawRunEnd(void)", 1)[0]
        self.assertIn("for (i = 0; i < 3; ++i)", section)
        self.assertIn("stageRewardBox", section)

    def test_stage_clear_next_fights_are_text_only(self):
        section = self.ui.split("static void drawStageClear(void)", 1)[1]
        section = section.split("static void drawRunEnd(void)", 1)[0]
        self.assertIn("stageFightBox", section)
        self.assertNotIn("ui_fighter_icon", section)

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
