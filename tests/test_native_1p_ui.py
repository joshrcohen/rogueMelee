import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

class NativeOnePlayerUiTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.ui = (ROOT / "mod/rogue_ui.c").read_text(encoding="utf-8")
        cls.rogue = (ROOT / "mod/rogue.c").read_text(encoding="utf-8")

    def test_combined_progression_screen(self):
        self.assertIn('"CHOOSE UPGRADE"', self.ui)
        self.assertIn('"CHOOSE NEXT MATCH"', self.ui)
        self.assertIn('"TOTAL SCORE"', self.ui)
        self.assertIn("ROUTE_UI_UPGRADE", self.ui)
        self.assertIn("ROUTE_UI_FIGHT", self.ui)

    def test_native_classic_matchup_intro(self):
        self.assertIn("GS_INTRO_EASY, &stage_intro", self.rogue)
        self.assertIn("lbDvdPreload_3", self.rogue)
        self.assertIn("stage_intro.allies[0] = g_rogue_run.player_kind", self.rogue)
        self.assertIn("stage_intro.enemies[i] = encounter->enemies[i].kind", self.rogue)

    def test_route_progression_language(self):
        self.assertIn('"ROGUE ROUTE"', self.ui)
        self.assertIn('"CHOOSE NEXT MATCH"', self.ui)
        self.assertIn('"BOSS"', self.ui)
        self.assertIn('"SHOP"', self.ui)
        self.assertIn('"FLOOR %d / %d"', self.ui)

    def test_native_continue_screen(self):
        self.assertIn("GS_GAMEOVER, &game_over_data, &game_over_data", self.rogue)
        self.assertIn("enterGameOver", self.rogue)
        self.assertIn("exitGameOver", self.rogue)

    def test_hud_is_simplified(self):
        self.assertNotIn('"A%d-%d  G%d  DMG %+.0f%%  RUN %+.0f%%  SH %+.0f%%"', self.ui)
        self.assertIn('"ACT %d-%d   G%d   %s"', self.ui)

if __name__ == "__main__":
    unittest.main()
