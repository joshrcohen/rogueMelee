import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class NativeOnePlayerUiTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.progress = (ROOT / "mod/rogue_progression.c").read_text(encoding="utf-8")
        cls.rogue = (ROOT / "mod/rogue.c").read_text(encoding="utf-8")

    def test_render_matches_target_sections(self):
        self.assertIn('"CHOOSE UPGRADE"', self.progress)
        self.assertIn('"CHOOSE NEXT FIGHT"', self.progress)
        self.assertIn('"CURRENT CHARACTER BUILD / UPGRADES"', self.progress)
        self.assertIn('"GOLD +%d  |  TOTAL %d  |  SCORE %d', self.progress)

    def test_three_upgrade_cards_exist(self):
        self.assertIn("static const float card_x[3]", self.progress)
        self.assertIn("draw_reward_text", self.progress)
        self.assertIn("draw_reward_panels", self.progress)

    def test_cards_disappear_after_upgrade(self):
        self.assertIn("has_reward && !upgrade_chosen", self.progress)
        self.assertIn('"LOCKED: %s"', self.progress)

    def test_fight_choice_uses_native_intro_models(self):
        self.assertIn("left = &round->choices[0];", self.progress)
        self.assertIn("right = &round->choices[1];", self.progress)
        self.assertIn("g_rogue_progression_intro.allies", self.progress)
        self.assertIn("g_rogue_progression_intro.enemies", self.progress)
        self.assertNotIn("ifStock_802F96D0", self.progress)

    def test_native_real_matchup_intro_remains(self):
        self.assertIn("GS_INTRO_EASY, &stage_intro", self.rogue)

    def test_native_continue_screen_remains(self):
        self.assertIn("GS_GAMEOVER, &game_over_data, &game_over_data", self.rogue)

    def test_v4_uses_native_640x480_coordinates(self):
        self.assertIn("640.0f", self.progress)
        self.assertIn("480.0f", self.progress)
        self.assertIn("text->font_size.x = 1.0f", self.progress)


if __name__ == "__main__":
    unittest.main()
