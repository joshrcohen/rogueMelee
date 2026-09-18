import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

class NativeOnePlayerUiTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.ui = (ROOT / "mod/rogue_ui.c").read_text(encoding="utf-8")

    def test_stage_clear_reward_screen(self):
        self.assertIn('"STAGE CLEAR"', self.ui)
        self.assertIn('"SPECIAL BONUS"', self.ui)
        self.assertIn('"CHOOSE A REWARD"', self.ui)

    def test_adventure_style_intro(self):
        self.assertIn('"NOW LOADING"', self.ui)
        self.assertIn('"VS"', self.ui)
        self.assertIn('RogueRoute_CharacterName(g_rogue_run.player_kind)', self.ui)

    def test_all_star_route_language(self):
        self.assertIn('"ALL-STAR PROGRESSION"', self.ui)
        self.assertIn('"CHOOSE NEXT MATCH"', self.ui)
        self.assertTrue(
            '"FINAL MATCH"' in self.ui or '"FINAL: %s"' in self.ui,
            "Route UI should identify the final boss match",
        )

    def test_continue_style_run_end(self):
        self.assertIn('"CONTINUE?"', self.ui)
        self.assertIn('"RUN SUMMARY"', self.ui)

    def test_hud_is_simplified(self):
        self.assertNotIn('"A%d-%d  G%d  DMG %+.0f%%  RUN %+.0f%%  SH %+.0f%%"', self.ui)
        self.assertIn('"ACT %d-%d   G%d   %s"', self.ui)

if __name__ == "__main__":
    unittest.main()
